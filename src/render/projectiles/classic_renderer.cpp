// 本文件实现共享网格、持久实例缓冲、视锥剔除和双色苦无光效；不访问 Game。
#include "classic_renderer.hpp"
#include "gl_state.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace koumakyou::projectiles {
namespace {
struct Vertex { Vector3 position,normal;float mask; };
struct Packed { float posScale[4],rotation[4],body[4],highlight[4],glow[4],effects[4]; };
void rgba(float* p,Color c){p[0]=c.r/255.F;p[1]=c.g/255.F;p[2]=c.b/255.F;p[3]=c.a/255.F;}
std::array<Vector4,6> planes(Matrix m){
    std::array<Vector4,6> p{{{m.m3+m.m0,m.m7+m.m4,m.m11+m.m8,m.m15+m.m12},
        {m.m3-m.m0,m.m7-m.m4,m.m11-m.m8,m.m15-m.m12},
        {m.m3+m.m1,m.m7+m.m5,m.m11+m.m9,m.m15+m.m13},
        {m.m3-m.m1,m.m7-m.m5,m.m11-m.m9,m.m15-m.m13},
        {m.m3+m.m2,m.m7+m.m6,m.m11+m.m10,m.m15+m.m14},
        {m.m3-m.m2,m.m7-m.m6,m.m11-m.m10,m.m15-m.m14}}};
    for(auto& q:p){float d=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z);q=Vector4Scale(q,1/d);}return p;
}
bool valid(const ProjectileInstance& i){
    const float norm=QuaternionLength(i.rotation);
    return std::isfinite(i.position.x)&&std::isfinite(i.position.y)&&std::isfinite(i.position.z)&&
        std::isfinite(i.scale)&&i.scale>0&&std::isfinite(norm)&&std::abs(norm-1)<.001F&&
        std::isfinite(i.appearance.glowStrength)&&i.appearance.glowStrength>=0&&i.appearance.glowStrength<=2&&
        std::isfinite(i.appearance.glowWidth)&&i.appearance.glowWidth>=0&&i.appearance.glowWidth<=.025F;
}
}
ProjectileAppearance ClassicPalette(int color){
    static constexpr Color palette[]{{156,163,176,255},{186,24,49,255},{255,51,68,255},{143,39,226,255},
        {241,48,241,255},{35,52,219,255},{90,82,255,255},{0,170,190,255},{54,230,255,255},
        {21,156,34,255},{72,248,35,255},{159,231,15,255},{220,165,13,255},{247,245,33,255},
        {255,138,24,255},{195,203,219,255}};
    if(color<0||color>15)throw std::invalid_argument("Color index must be 0..15");
    ProjectileAppearance a;a.bodyColor=palette[color];a.glowColor=palette[color];return a;
}
struct ClassicRenderer::Impl {
    GLuint vao=0,vertex=0,index=0,instances=0;Shader shader{};GLsizei indexCount=0;
    std::vector<Packed> packed;std::size_t capacity;BatchStats stats{};Matrix vp{};bool anyGlow=false;
    int vpLoc,passLoc,clayLoc;float bound=0;
    explicit Impl(std::size_t n):packed(n),capacity(n){}
};
ClassicRenderer::ClassicRenderer(const std::filesystem::path& asset,const std::filesystem::path& shaders,std::size_t capacity):impl_(std::make_unique<Impl>(capacity)){
    if(!capacity)throw std::invalid_argument("Empty instance capacity");
    auto& p=*impl_;GLState state;
    Model model=LoadModel(asset.string().c_str());
    if(model.meshCount!=1){UnloadModel(model);throw std::runtime_error("Classic asset requires exactly one render mesh");}
    const Mesh& mesh=model.meshes[0];
    if(mesh.triangleCount>192||!mesh.colors||!mesh.normals){UnloadModel(model);throw std::runtime_error("Classic mesh budget or color-mask violation");}
    std::vector<Vertex> vertices(mesh.vertexCount);
    for(int i=0;i<mesh.vertexCount;++i){
        vertices[i]={{mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]},
                     {mesh.normals[i*3],mesh.normals[i*3+1],mesh.normals[i*3+2]},mesh.colors[i*4]/255.F};
        p.bound=std::max(p.bound,Vector3Length(vertices[i].position));
    }
    std::vector<unsigned short> indices(mesh.triangleCount*3);
    for(std::size_t i=0;i<indices.size();++i)indices[i]=mesh.indices?mesh.indices[i]:static_cast<unsigned short>(i);
    p.indexCount=static_cast<GLsizei>(indices.size());UnloadModel(model);
    p.shader=LoadShader((shaders/"classic.vs").string().c_str(),(shaders/"classic.fs").string().c_str());
    if(p.shader.id==rlGetShaderIdDefault())throw std::runtime_error("Classic shader compilation failed");
    p.vpLoc=glGetUniformLocation(p.shader.id,"viewProjection");p.passLoc=glGetUniformLocation(p.shader.id,"glowPass");p.clayLoc=glGetUniformLocation(p.shader.id,"clay");
    glGenVertexArrays(1,&p.vao);glBindVertexArray(p.vao);
    glGenBuffers(1,&p.vertex);glBindBuffer(GL_ARRAY_BUFFER,p.vertex);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
    for(int a=0;a<3;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,a==2?1:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(a==0?0:a==1?sizeof(Vector3):sizeof(Vector3)*2));}
    glGenBuffers(1,&p.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned short),indices.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&p.instances);glBindBuffer(GL_ARRAY_BUFFER,p.instances);glBufferData(GL_ARRAY_BUFFER,capacity*sizeof(Packed),nullptr,GL_DYNAMIC_DRAW);
    for(int a=3;a<9;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(Packed),reinterpret_cast<void*>(sizeof(float)*4*(a-3)));glVertexAttribDivisor(a,1);}
}
ClassicRenderer::~ClassicRenderer(){if(!impl_)return;auto& p=*impl_;glDeleteBuffers(1,&p.instances);glDeleteBuffers(1,&p.index);glDeleteBuffers(1,&p.vertex);glDeleteVertexArrays(1,&p.vao);UnloadShader(p.shader);}
void ClassicRenderer::Prepare(std::span<const ProjectileInstance> instances,const Camera3D& camera,float aspect,bool cull){
    auto& p=*impl_;if(instances.size()>p.capacity)throw std::length_error("Instance capacity exceeded");
    if(!(aspect>0)||!std::isfinite(aspect))throw std::invalid_argument("Invalid camera aspect");
    p.vp=MatrixMultiply(GetCameraMatrix(camera),MatrixPerspective(camera.fovy*DEG2RAD,aspect,.01,1000));
    auto frustum=planes(p.vp);p.stats={instances.size(),0,0,0};p.anyGlow=false;
    for(const auto& i:instances){
        if(!valid(i))throw std::invalid_argument("Invalid projectile instance");
        bool visible=true;
        if(cull)for(auto q:frustum)if(q.x*i.position.x+q.y*i.position.y+q.z*i.position.z+q.w<-(p.bound+i.appearance.glowWidth)*i.scale){visible=false;break;}
        if(!visible)continue;
        Packed& x=p.packed[p.stats.visible++];x.posScale[0]=i.position.x;x.posScale[1]=i.position.y;x.posScale[2]=i.position.z;x.posScale[3]=i.scale;
        x.rotation[0]=i.rotation.x;x.rotation[1]=i.rotation.y;x.rotation[2]=i.rotation.z;x.rotation[3]=i.rotation.w;
        rgba(x.body,i.appearance.bodyColor);rgba(x.highlight,i.appearance.highlightColor);rgba(x.glow,i.appearance.glowColor);
        x.effects[0]=i.appearance.glowStrength;x.effects[1]=i.appearance.glowWidth;x.effects[2]=x.effects[3]=0;
        p.anyGlow|=i.appearance.glowStrength>0&&i.appearance.glowWidth>0;
    }
    GLint previous;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,p.instances);
    glBufferSubData(GL_ARRAY_BUFFER,0,p.stats.visible*sizeof(Packed),p.packed.data());glBindBuffer(GL_ARRAY_BUFFER,previous);
}
void ClassicRenderer::Draw(bool glow,bool clay){
    auto& p=*impl_;if(!p.stats.visible)return;rlDrawRenderBatchActive();GLState state;
    glBindVertexArray(p.vao);glUseProgram(p.shader.id);
    auto matrix=MatrixToFloatV(p.vp);glUniformMatrix4fv(p.vpLoc,1,GL_FALSE,matrix.v);glUniform1i(p.clayLoc,clay);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    glUniform1i(p.passLoc,0);glDrawElementsInstanced(GL_TRIANGLES,p.indexCount,GL_UNSIGNED_SHORT,nullptr,static_cast<int>(p.stats.visible));
    p.stats.drawCalls=1;p.stats.triangles=static_cast<int>(p.stats.visible)*p.indexCount/3;
    if(glow&&p.anyGlow&&!clay){
        glUniform1i(p.passLoc,1);glDepthMask(GL_FALSE);glCullFace(GL_FRONT);glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_ZERO,GL_ONE);
        glDrawElementsInstanced(GL_TRIANGLES,p.indexCount,GL_UNSIGNED_SHORT,nullptr,static_cast<int>(p.stats.visible));
        ++p.stats.drawCalls;p.stats.triangles*=2;
    }
}
const BatchStats& ClassicRenderer::Stats()const{return impl_->stats;}
std::size_t ClassicRenderer::BufferBytes()const{return impl_->capacity*sizeof(Packed);}
bool TouchesInstance(const CollisionAsset& asset,CollisionScratch& scratch,const ProjectileInstance& i,Vector3 point,float radius,std::size_t* narrow){
    if(!(i.scale>0)||!std::isfinite(i.scale)||!std::isfinite(radius)||radius<0)throw std::invalid_argument("Invalid collision scale/radius");
    auto delta=Vector3Subtract(point,i.position);float bound=asset.BoundingRadius()*i.scale+radius;
    if(Vector3LengthSqr(delta)>bound*bound)return false;
    if(narrow)++*narrow;
    Vector3 local=Vector3Scale(Vector3RotateByQuaternion(delta,QuaternionInvert(i.rotation)),1/i.scale);
    float r=radius/i.scale;return asset.DistanceSquared(local,scratch)<=r*r;
}
} // namespace koumakyou::projectiles
