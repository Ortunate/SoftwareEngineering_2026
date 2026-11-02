// 本文件实现按实体/透明壳/火焰角色分组的持久实例绘制，透明能量层使用顺序无关加色。
#include "resources/projectile_paths.hpp"
#include "collection_renderer.hpp"
#include "gl_state.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>
namespace koumakyou::projectiles {
const std::array<ProjectileKind,10>& ProjectileKinds(){
    static const std::array<ProjectileKind,10> kinds{{
        {"pellet_classic","PELLET",16,160},{"ring_orb_classic","RING ORB",16,320},
        {"rice_classic","RICE",16,160},{"orb_classic","SOLID ORB",16,320},
        {"kunai_classic","KUNAI",16,192},{"needle_classic","NEEDLE CRYSTAL",16,96},
        {"large_orb_classic","LARGE ORB",8,512},{"fireball_classic","FIREBALL",1,384},
        {"knife_classic","SHORT KNIFE",8,256},{"bubble_classic","BUBBLE",5,768}}};return kinds;
}
ProjectileAppearance AppearanceFor(int type,int color){
    if(type<0||type>9||color<0||color>15)throw std::invalid_argument("Invalid native type/color");
    static const int eight[]{0,2,4,5,8,9,13,15},five[]{2,5,10,12,15};
    int index=type==7?2:type==9?five[color%5]:(type==6||type==8)?eight[color%8]:color;
    auto result=ClassicPalette(index);if(type==7)result.bodyColor={255,69,12,255},result.highlightColor={255,253,215,255},result.glowColor={255,58,8,255};
    if(type==0)result.glowWidth=.009F;return result;
}
namespace {
struct Vertex {Vector3 p,n;float mask,phase;};
struct Packed {float position[4],rotation[4],body[4],highlight[4],glow[4],effects[4];};
void rgba(float* p,Color c){p[0]=c.r/255.F;p[1]=c.g/255.F;p[2]=c.b/255.F;p[3]=c.a/255.F;}
bool finite(Vector3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
std::array<Vector4,6> planes(Matrix m){
    std::array<Vector4,6> p{{{m.m3+m.m0,m.m7+m.m4,m.m11+m.m8,m.m15+m.m12},{m.m3-m.m0,m.m7-m.m4,m.m11-m.m8,m.m15-m.m12},
        {m.m3+m.m1,m.m7+m.m5,m.m11+m.m9,m.m15+m.m13},{m.m3-m.m1,m.m7-m.m5,m.m11-m.m9,m.m15-m.m13},
        {m.m3+m.m2,m.m7+m.m6,m.m11+m.m10,m.m15+m.m14},{m.m3-m.m2,m.m7-m.m6,m.m11-m.m10,m.m15-m.m14}}};
    for(auto& q:p){float d=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z);q=Vector4Scale(q,1/d);}return p;
}
}
struct CollectionRenderer::Impl {
    struct Geometry {GLuint vao=0,vertices=0,indices=0;std::array<int,3> offset{},count{};float radius=0;std::unique_ptr<CollisionAsset> collision;};
    std::array<Geometry,10> geometries;std::array<size_t,10> count{},start{},cursor{};
    std::vector<Packed> packed;std::vector<int> visibility;GLuint instances=0;Shader shader{};Matrix vp{};Vector3 eye{};
    // start/count 的单位是实例记录而非字节，范围只在本次 BeginFrame 到下次 BeginFrame 之间有效。
    struct Batch {std::array<size_t,10> count,start;BatchStats stats;bool translucent;};
    std::vector<Batch> batches;std::array<Vector4,6> frustum{};size_t used=0;bool uploaded=false;
    int vpLoc,eyeLoc,secondsLoc,modeLoc,kindLoc,passLoc,roleLoc;
    size_t capacity;BatchStats stats{};bool translucent=false;
    explicit Impl(size_t n):packed(n*2),visibility(n),capacity(n){batches.reserve(n*2+1);}
    ~Impl(){for(auto& g:geometries){if(g.vao)glDeleteVertexArrays(1,&g.vao);if(g.vertices)glDeleteBuffers(1,&g.vertices);if(g.indices)glDeleteBuffers(1,&g.indices);}if(instances)glDeleteBuffers(1,&instances);if(shader.id)UnloadShader(shader);}
};
CollectionRenderer::CollectionRenderer(const std::filesystem::path& root,size_t capacity):impl_(std::make_unique<Impl>(capacity)){
    if(!capacity)throw std::invalid_argument("Empty capacity");rlDrawRenderBatchActive();GLState guard;auto& p=*impl_;
    p.shader=LoadShader((ProjectileShaderDirectory(root)/"collection.vs").string().c_str(),(ProjectileShaderDirectory(root)/"collection.fs").string().c_str());
    if(p.shader.id==rlGetShaderIdDefault()){p.shader={};throw std::runtime_error("Collection shader failed");}
    p.vpLoc=glGetUniformLocation(p.shader.id,"viewProjection");p.eyeLoc=glGetUniformLocation(p.shader.id,"eye");
    p.secondsLoc=glGetUniformLocation(p.shader.id,"seconds");p.modeLoc=glGetUniformLocation(p.shader.id,"mode");
    p.kindLoc=glGetUniformLocation(p.shader.id,"kind");p.passLoc=glGetUniformLocation(p.shader.id,"pass");p.roleLoc=glGetUniformLocation(p.shader.id,"role");
    glGenBuffers(1,&p.instances);glBindBuffer(GL_ARRAY_BUFFER,p.instances);glBufferData(GL_ARRAY_BUFFER,capacity*2*sizeof(Packed),nullptr,GL_DYNAMIC_DRAW);
    for(int kind=0;kind<10;++kind){auto& g=p.geometries[kind];auto dir=ProjectileAssetDirectory(root)/ProjectileKinds()[kind].directory;
        g.collision=std::make_unique<CollisionAsset>(dir/"collision.bin");std::string name=ProjectileKinds()[kind].directory;
        Model model=LoadModel((dir/(name+".gltf")).string().c_str());
        if(model.meshCount!=1){UnloadModel(model);throw std::runtime_error("One joined mesh required: "+name);}
        auto& m=model.meshes[0];if(!m.colors||!m.normals||m.triangleCount>ProjectileKinds()[kind].hardLimit){UnloadModel(model);throw std::runtime_error("Mesh attributes/budget invalid: "+name);}
        std::vector<Vertex> vertices(m.vertexCount);std::array<std::vector<unsigned short>,3> groups;
        for(int i=0;i<m.vertexCount;++i){auto& v=vertices[i];v={{m.vertices[i*3],m.vertices[i*3+1],m.vertices[i*3+2]},{m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]},m.colors[i*4]/255.F,m.colors[i*4+2]/255.F};g.radius=std::max(g.radius,Vector3Length(v.p));}
        for(int t=0;t<m.triangleCount;++t){int first=m.indices?m.indices[t*3]:t*3;int role=kind==4?0:std::clamp((int)std::round(m.colors[first*4+1]*2.F/255),0,2);
            for(int j=0;j<3;++j)groups[role].push_back(m.indices?m.indices[t*3+j]:t*3+j);}
        UnloadModel(model);std::vector<unsigned short> indices;
        for(int role=0;role<3;++role){g.offset[role]=(int)indices.size();g.count[role]=(int)groups[role].size();indices.insert(indices.end(),groups[role].begin(),groups[role].end());}
        glGenVertexArrays(1,&g.vao);glBindVertexArray(g.vao);glGenBuffers(1,&g.vertices);glBindBuffer(GL_ARRAY_BUFFER,g.vertices);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)sizeof(Vector3));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(sizeof(Vector3)*2));
        glGenBuffers(1,&g.indices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.indices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*2,indices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,p.instances);for(int a=3;a<9;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(Packed),(void*)(sizeof(float)*4*(a-3)));glVertexAttribDivisor(a,1);}
    }
}
CollectionRenderer::~CollectionRenderer()=default;
void CollectionRenderer::Prepare(std::span<const CollectionInstance> items,const Camera3D& camera,float aspect,bool cull){
    BeginFrame(camera,aspect);Append(items,cull);Upload();Select(0);
}
void CollectionRenderer::BeginFrame(const Camera3D& camera,float aspect){
    auto& p=*impl_;
    if(!(aspect>0)||!std::isfinite(aspect)||!finite(camera.position)||!finite(camera.target)||camera.projection!=CAMERA_PERSPECTIVE)throw std::invalid_argument("Invalid perspective camera");
    p.vp=MatrixMultiply(GetCameraMatrix(camera),MatrixPerspective(camera.fovy*DEG2RAD,aspect,.01,1000));p.eye=camera.position;
    p.frustum=planes(p.vp);p.used=0;p.batches.clear();p.uploaded=false;
}
std::size_t CollectionRenderer::Append(std::span<const CollectionInstance> items,bool cull){
    auto& p=*impl_;if(p.uploaded||items.size()>p.capacity||p.used+items.size()>p.packed.size())throw std::length_error("Shared projectile frame capacity exceeded");
    const auto& frustum=p.frustum;p.count.fill(0);p.stats={items.size(),0,0,0};p.translucent=false;
    for(size_t j=0;j<items.size();++j){auto& item=items[j];auto& i=item.transform;auto& a=i.appearance;p.translucent|=a.bodyColor.a<255;float norm=QuaternionLength(i.rotation);
        if(item.type<0||item.type>9||!finite(i.position)||!(i.scale>0)||!std::isfinite(i.scale)||!std::isfinite(norm)||std::abs(norm-1)>.001F||
            !std::isfinite(a.glowStrength)||a.glowStrength<0||a.glowStrength>2||!std::isfinite(a.glowWidth)||a.glowWidth<0||a.glowWidth>.025F||
            !std::isfinite(item.shellOpacity)||item.shellOpacity<0||item.shellOpacity>1||!std::isfinite(item.phase))throw std::invalid_argument("Invalid collection instance");
        float radius=(p.geometries[item.type].radius+a.glowWidth+(item.type==7?.14F:0))*i.scale;bool visible=true;
        if(cull)for(auto q:frustum)if(q.x*i.position.x+q.y*i.position.y+q.z*i.position.z+q.w<-radius){visible=false;break;}
        p.visibility[j]=visible?item.type:-1;if(visible)++p.count[item.type],++p.stats.visible;
    }
    size_t sum=p.used;for(int k=0;k<10;++k){p.start[k]=p.cursor[k]=sum;sum+=p.count[k];}
    for(size_t j=0;j<items.size();++j){int k=p.visibility[j];if(k<0)continue;auto& item=items[j];auto& i=item.transform;auto& x=p.packed[p.cursor[k]++];
        x.position[0]=i.position.x;x.position[1]=i.position.y;x.position[2]=i.position.z;x.position[3]=i.scale;
        x.rotation[0]=i.rotation.x;x.rotation[1]=i.rotation.y;x.rotation[2]=i.rotation.z;x.rotation[3]=i.rotation.w;
        rgba(x.body,i.appearance.bodyColor);rgba(x.highlight,i.appearance.highlightColor);rgba(x.glow,i.appearance.glowColor);
        x.effects[0]=i.appearance.glowStrength;x.effects[1]=i.appearance.glowWidth;x.effects[2]=item.shellOpacity;x.effects[3]=item.phase;
    }
    p.used=sum;p.batches.push_back({p.count,p.start,p.stats,p.translucent});return p.batches.size()-1;
}
void CollectionRenderer::Upload(){
    auto& p=*impl_;if(p.uploaded)throw std::logic_error("Projectile frame already uploaded");
    rlDrawRenderBatchActive();GLint previous;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,p.instances);
    // 保留预分配存储并由驱动隐式同步；M5 长测中反复重命名会拉长 GPU 区间，不能只看 CPU 提交时间。
    if(p.used)glBufferSubData(GL_ARRAY_BUFFER,0,p.used*sizeof(Packed),p.packed.data());
    glBindBuffer(GL_ARRAY_BUFFER,previous);p.uploaded=true;
}
void CollectionRenderer::Select(std::size_t batch){
    auto& p=*impl_;if(!p.uploaded)throw std::logic_error("Projectile frame not uploaded");const auto& b=p.batches.at(batch);
    p.count=b.count;p.start=b.start;p.stats=b.stats;p.translucent=b.translucent;
}
void CollectionRenderer::Draw(float seconds,bool glow,int mode,int firstPass,int lastPass){
    if(!std::isfinite(seconds)||seconds<0||mode<0||mode>6||firstPass<0||lastPass>2||firstPass>lastPass)throw std::invalid_argument("Invalid draw time/mode");auto& p=*impl_;p.stats.drawCalls=p.stats.triangles=0;if(!p.stats.visible)return;
    rlDrawRenderBatchActive();GLState state;glUseProgram(p.shader.id);auto matrix=MatrixToFloatV(p.vp);glUniformMatrix4fv(p.vpLoc,1,GL_FALSE,matrix.v);
    glUniform3f(p.eyeLoc,p.eye.x,p.eye.y,p.eye.z);glUniform1f(p.secondsLoc,seconds);glUniform1i(p.modeLoc,mode==4?1:mode);
    GLint polygon[2]{};if(mode==4){glGetIntegerv(GL_POLYGON_MODE,polygon);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);}
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);glFrontFace(GL_CCW);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    // 全部不透明实体先完成深度，随后是无深度写入的透明能量层，最后可选外轮廓光。
    for(int pass=firstPass;pass<=lastPass;++pass){if(pass==1&&(mode==1||mode==4))continue;if(pass==2&&(!glow||mode!=0))continue;if(pass==0&&(mode==3||mode==5||mode==6))continue;
        if(pass==0){glDepthMask(p.translucent?GL_FALSE:GL_TRUE);if(p.translucent){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);}else glDisable(GL_BLEND);glCullFace(GL_BACK);}else{glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_ZERO,GL_ONE);glCullFace(pass==2?GL_FRONT:GL_BACK);}
        for(int k=0;k<10;++k){if(!p.count[k])continue;auto& g=p.geometries[k];glBindVertexArray(g.vao);glBindBuffer(GL_ARRAY_BUFFER,p.instances);
            for(int a=3;a<9;++a)glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(Packed),(void*)(p.start[k]*sizeof(Packed)+sizeof(float)*4*(a-3)));
            glUniform1i(p.kindLoc,k);glUniform1i(p.passLoc,pass);
            int glowRole=(k==1||k==3||k==6||k==9)?1:k==7?2:0;
            for(int role=0;role<3;++role){bool draw=pass==0?(mode==1||mode==4||role==0):pass==1?(role>0||k==9):role==glowRole;if(!draw||!g.count[role])continue;
                // 泡内球在透明阶段单独吸收背景；不写深度，不遮断其他装饰层。
                if(k==9&&role==0&&mode!=1&&mode!=4&&pass==0)continue;
                if(pass==1&&mode==2&&role!=0)continue;
                if(mode==3&&role==0)continue;
                if(mode==5&&role!=1)continue;if(mode==6&&role!=2)continue;
                if(pass==1)glBlendFuncSeparate(k==9&&role==0?GL_ZERO:GL_SRC_ALPHA,k==9&&role==0?GL_ONE_MINUS_SRC_ALPHA:GL_ONE,GL_ZERO,GL_ONE);
                if(pass==1&&role==1)glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);
                glUniform1i(p.roleLoc,role);glDrawElementsInstanced(GL_TRIANGLES,g.count[role],GL_UNSIGNED_SHORT,(void*)(g.offset[role]*sizeof(unsigned short)),(int)p.count[k]);
                ++p.stats.drawCalls;p.stats.triangles+=g.count[role]/3*(int)p.count[k];
            }
        }
    }
    if(mode==4)glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
}
const BatchStats& CollectionRenderer::Stats()const{return impl_->stats;}
const CollisionAsset& CollectionRenderer::Collision(int type)const{if(type<0||type>9)throw std::invalid_argument("Invalid type");return *impl_->geometries[type].collision;}
int CollectionRenderer::Triangles(int type)const{if(type<0||type>9)throw std::invalid_argument("Invalid type");auto& n=impl_->geometries[type].count;return (n[0]+n[1]+n[2])/3;}
size_t CollectionRenderer::BufferBytes()const{return impl_->packed.size()*sizeof(Packed);}
} // namespace koumakyou::projectiles
