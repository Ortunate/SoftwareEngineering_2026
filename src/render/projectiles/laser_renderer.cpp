// 本文件实现两套激光持久实例批绘制，以及与游戏一致的胶囊和时窗语义。
#include "resources/projectile_paths.hpp"
#include "laser_renderer.hpp"
#include "gl_state.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace koumakyou::projectiles {
void ValidateLaser(const LaserInstance& b){
    if(!std::isfinite(b.visualScaleOverride)||(b.visualScaleOverride<0&&b.visualScaleOverride!= -1)||b.dangerOverride< -1||b.dangerOverride>1)throw std::invalid_argument("Invalid runtime laser sample");
    if(!std::isfinite(b.fadeSeconds)||b.fadeSeconds<0||b.fadeSeconds>b.end)throw std::invalid_argument("Invalid fade duration");
    auto finite=[](Vector3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    if(!finite(b.origin)||!finite(b.direction)||std::abs(Vector3Length(b.direction)-1)>.001F||!std::isfinite(b.length)||b.length<=0||!std::isfinite(b.radius)||b.radius<=0||b.variant<0||b.variant>1||b.color<0||b.color>15||!std::isfinite(b.age)||b.age<0||!std::isfinite(b.end)||!(b.warning>=0&&b.growEnd>=b.warning&&b.dangerEnd>=b.growEnd&&b.end>=b.dangerEnd)||!std::isfinite(b.glowStrength)||b.glowStrength<0||b.glowStrength>2)throw std::invalid_argument("Invalid laser parameters");
}
bool LaserDangerous(const LaserInstance& b){return b.age>=b.warning&&b.age<b.dangerEnd&&b.age<b.end;}
float LaserVisualScale(const LaserInstance& b){
    // 与 Bullet::BeamVisualScale 保持相同的展开/收束规则，危险时窗单独控制。
    if(b.age>=b.end)return 0;
    if(b.growEnd<=0)return LaserDangerous(b)?1:.3F;
    if(b.age<b.growEnd-.5F)return .075F;
    if(b.age<b.growEnd)return std::clamp(b.age/b.growEnd,.075F,1.F);
    // 原生淡出起点与危险截止可以不同，不能用 dangerEnd 反推视觉收束时长。
    float fade=b.fadeSeconds;
    if(fade>0&&b.age>b.end-fade)return std::clamp((b.end-b.age)/fade,0.F,1.F);
    return 1;
}
bool LaserTouches(const LaserInstance& b,Vector3 probe,float radius){
    ValidateLaser(b);if(!std::isfinite(radius)||radius<0||!std::isfinite(probe.x)||!std::isfinite(probe.y)||!std::isfinite(probe.z))throw std::invalid_argument("Invalid probe");
    Vector3 d=Vector3Subtract(probe,b.origin);float t=std::clamp(Vector3DotProduct(d,b.direction),0.F,b.length);
    return LaserDangerous(b)&&Vector3LengthSqr(Vector3Subtract(d,Vector3Scale(b.direction,t)))<=(radius+b.radius)*(radius+b.radius);
}
struct LaserRenderer::Impl {
    struct V{Vector3 p,n;};struct Packed{Vector4 origin,direction,color,fx;};
    struct Geometry{GLuint vao=0,vbo=0,ebo=0;int offset[2]{},count[2]{};};
    Geometry geometry[2];GLuint instances=0;Shader shader{};std::vector<Packed> packed;size_t capacity;BatchStats stats{};
    // indices 将输入索引映射到按 variant 分段的 Packed 下标；capacity 标记已过期条目，每次 Prepare 重建。
    std::vector<std::size_t> indices;std::array<size_t,2> count{},start{};Matrix vp{};Vector3 eye{};
    int vpLoc,eyeLoc,modeLoc,fromLoc,toLoc,variantLoc,passLoc;
    explicit Impl(size_t n):packed(n),capacity(n){indices.reserve(n);}
    void Render(int variant,size_t first,size_t count,int mode,bool glow,float from,float to);
    ~Impl(){for(auto& g:geometry){glDeleteVertexArrays(1,&g.vao);glDeleteBuffers(1,&g.vbo);glDeleteBuffers(1,&g.ebo);}glDeleteBuffers(1,&instances);UnloadShader(shader);}
};
LaserRenderer::LaserRenderer(const std::filesystem::path& root,size_t capacity):p_(std::make_unique<Impl>(capacity)){
    if(!capacity)throw std::invalid_argument("Empty laser capacity");GLState guard;auto& p=*p_;
    p.shader=LoadShader((ProjectileShaderDirectory(root)/"laser.vs").string().c_str(),(ProjectileShaderDirectory(root)/"laser.fs").string().c_str());
    if(p.shader.id==rlGetShaderIdDefault()){p.shader={};throw std::runtime_error("Laser shader failed");}
    p.vpLoc=glGetUniformLocation(p.shader.id,"vp");p.eyeLoc=glGetUniformLocation(p.shader.id,"eye");p.modeLoc=glGetUniformLocation(p.shader.id,"mode");
    p.fromLoc=glGetUniformLocation(p.shader.id,"clipFrom");p.toLoc=glGetUniformLocation(p.shader.id,"clipTo");p.variantLoc=glGetUniformLocation(p.shader.id,"variant");p.passLoc=glGetUniformLocation(p.shader.id,"pass");
    glGenBuffers(1,&p.instances);glBindBuffer(GL_ARRAY_BUFFER,p.instances);glBufferData(GL_ARRAY_BUFFER,capacity*sizeof(Impl::Packed),nullptr,GL_DYNAMIC_DRAW);
    const char* names[]{"laser_default","laser_round"};
    for(int k=0;k<2;++k){auto& g=p.geometry[k];auto path=ProjectileAssetDirectory(root)/names[k]/(std::string(names[k])+".gltf");Model m=LoadModel(path.string().c_str());
        if(m.meshCount!=1){UnloadModel(m);throw std::runtime_error("Laser mesh count");}auto& mesh=m.meshes[0];
        if(!mesh.colors||!mesh.normals||mesh.triangleCount>384){UnloadModel(m);throw std::runtime_error("Laser attributes/budget");}
        std::vector<Impl::V> v(mesh.vertexCount);for(int i=0;i<mesh.vertexCount;++i)v[i]={{mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]},{mesh.normals[i*3],mesh.normals[i*3+1],mesh.normals[i*3+2]}};
        std::vector<unsigned short> indices;
        for(int role=0;role<2;++role){g.offset[role]=(int)indices.size();for(int i=0;i<mesh.triangleCount;++i){int first=mesh.indices?mesh.indices[i*3]:i*3;if((mesh.colors[first*4+1]>127)!=bool(role))continue;for(int j=0;j<3;++j)indices.push_back(mesh.indices?mesh.indices[i*3+j]:i*3+j);}g.count[role]=(int)indices.size()-g.offset[role];}
        UnloadModel(m);glGenVertexArrays(1,&g.vao);glBindVertexArray(g.vao);glGenBuffers(1,&g.vbo);glBindBuffer(GL_ARRAY_BUFFER,g.vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(v[0]),v.data(),GL_STATIC_DRAW);
        for(int a=0;a<2;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,3,GL_FLOAT,GL_FALSE,sizeof(Impl::V),(void*)(a*sizeof(Vector3)));}
        glGenBuffers(1,&g.ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*2,indices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,p.instances);for(int a=2;a<6;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(Impl::Packed),(void*)((a-2)*sizeof(Vector4)));glVertexAttribDivisor(a,1);}
    }
}
LaserRenderer::~LaserRenderer()=default;
void LaserRenderer::Prepare(std::span<const LaserInstance> beams,const Camera3D& camera,float aspect){
    auto& p=*p_;if(beams.size()>p.capacity||!std::isfinite(aspect)||aspect<=0)throw std::invalid_argument("Laser prepare parameters");
    for(const auto& b:beams)ValidateLaser(b);
    p.vp=MatrixMultiply(GetCameraMatrix(camera),MatrixPerspective(camera.fovy*DEG2RAD,aspect,.01,1000));p.eye=camera.position;
    p.count={};p.indices.assign(beams.size(),p.capacity);p.stats={beams.size(),0,0,0};
    for(const auto& b:beams)if(b.age<b.end)++p.count[b.variant];
    p.start={0,p.count[0]};auto cursor=p.start;
    for(size_t i=0;i<beams.size();++i){const auto& b=beams[i];if(b.age>=b.end)continue;auto index=cursor[b.variant]++;p.indices[i]=index;auto c=b.useColorOverride?b.tintOverride:ClassicPalette(b.color).bodyColor;auto& x=p.packed[index];
        x.origin={b.origin.x,b.origin.y,b.origin.z,b.length};x.direction={b.direction.x,b.direction.y,b.direction.z,b.radius*(b.visualScaleOverride>=0?b.visualScaleOverride:LaserVisualScale(b))};
        x.color={c.r/255.F,c.g/255.F,c.b/255.F,c.a/255.F};x.fx={b.glowStrength,b.dangerOverride>=0?float(b.dangerOverride):(LaserDangerous(b)?1.F:0.F),0,0};
    }
    p.stats.visible=p.count[0]+p.count[1];if(!p.stats.visible)return;
    rlDrawRenderBatchActive();GLint previous;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,p.instances);
    // 同帧所有片段引用这次上传；保留预分配存储，由驱动同步上一帧的读取。
    glBufferSubData(GL_ARRAY_BUFFER,0,p.stats.visible*sizeof(Impl::Packed),p.packed.data());glBindBuffer(GL_ARRAY_BUFFER,previous);
}
void LaserRenderer::Impl::Render(int variant,size_t first,size_t number,int mode,bool glow,float from,float to){
    if(!number)return;rlDrawRenderBatchActive();GLState guard;glUseProgram(shader.id);auto matrix=MatrixToFloatV(vp);
    glUniformMatrix4fv(vpLoc,1,GL_FALSE,matrix.v);glUniform3f(eyeLoc,eye.x,eye.y,eye.z);glUniform1i(modeLoc,mode);glUniform1f(fromLoc,from);glUniform1f(toLoc,to);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_ZERO,GL_ONE);glDepthMask(GL_FALSE);
    auto& g=geometry[variant];glBindVertexArray(g.vao);glBindBuffer(GL_ARRAY_BUFFER,instances);
    for(int a=2;a<6;++a)glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(Packed),reinterpret_cast<void*>(first*sizeof(Packed)+(a-2)*sizeof(Vector4)));
    glUniform1i(variantLoc,variant);
    for(int pass=0;pass<(glow&&mode==0?3:2);++pass){if(mode==2&&pass!=0)continue;if(mode==3&&pass!=1)continue;int role=pass==0?0:1;glUniform1i(passLoc,pass);glDrawElementsInstanced(GL_TRIANGLES,g.count[role],GL_UNSIGNED_SHORT,reinterpret_cast<void*>(g.offset[role]*sizeof(unsigned short)),int(number));++stats.drawCalls;stats.triangles+=g.count[role]/3*int(number);}
}
void LaserRenderer::Draw(std::span<const LaserInstance> beams,const Camera3D& camera,float aspect,int mode,bool glow,float from,float to){
    if(mode<0||mode>3||!std::isfinite(from)||!std::isfinite(to)||from<0||to>1||from>=to)throw std::invalid_argument("Laser draw parameters");
    Prepare(beams,camera,aspect);auto& p=*p_;for(int k=0;k<2;++k)p.Render(k,p.start[k],p.count[k],mode,glow,from,to);
}
void LaserRenderer::DrawPrepared(std::size_t index,float from,float to){
    if(!std::isfinite(from)||!std::isfinite(to)||from<0||to>1||from>=to)throw std::invalid_argument("Laser clip parameters");
    auto& p=*p_;const auto packed=p.indices.at(index);if(packed==p.capacity)return;p.stats.drawCalls=p.stats.triangles=0;
    p.Render(packed<p.start[1]?0:1,packed,1,0,true,from,to);
}
const BatchStats& LaserRenderer::Stats()const{return p_->stats;}
}
