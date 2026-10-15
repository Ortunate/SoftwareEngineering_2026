// 本文件将 ANM 的局部变换和图集 UV 投影到当前相机平面，统一保留原素材 Alpha。
#include "render/animated_sprite.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
namespace koumakyou {
namespace {
Vector3 Vector3Add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Vector3Subtract(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 Vector3Scale(Vector3 a,float s) { return {a.x*s,a.y*s,a.z*s}; }
float Vector3DotProduct(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float Vector3LengthSqr(Vector3 a) { return Vector3DotProduct(a,a); }
Vector3 Vector3Normalize(Vector3 a) { return Vector3Scale(a,1/std::sqrt(std::max(1e-12F,Vector3LengthSqr(a)))); }
Vector3 Vector3CrossProduct(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vector3 Rotate(Vector3 p,Vector3 angle) {
    const float cx=std::cos(angle.x),sx=std::sin(angle.x),cy=std::cos(angle.y),sy=std::sin(angle.y),cz=std::cos(angle.z),sz=std::sin(angle.z);
    p={p.x,p.y*cx-p.z*sx,p.y*sx+p.z*cx};
    p={p.x*cy+p.z*sy,p.y,-p.x*sy+p.z*cy};
    return {p.x*cz-p.y*sz,p.x*sz+p.y*cz,p.z};
}
Color Tint(Color a,Color b) {
    return {static_cast<unsigned char>(a.r*b.r/255),static_cast<unsigned char>(a.g*b.g/255),
            static_cast<unsigned char>(a.b*b.b/255),static_cast<unsigned char>(a.a*b.a/255)};
}
/// @brief quad 按左上、右上、右下、左下排序，UV 以 ANM 逻辑尺寸换算，不依赖 DDS 分辨率。
void Quad(Texture2D texture,const anm::Archive& archive,const anm::Frame& frame,
          const std::array<Vector3,4>& points,Color tint,bool screenSpace=false,bool mirrored=false) {
    if(!IsTextureValid(texture) || !frame.visible || frame.sprite<0)return;
    const auto s=frame.source;
    const float u0=s.x/archive.width+frame.uvScroll.x,u1=(s.x+s.width)/archive.width+frame.uvScroll.x;
    const float v0=s.y/archive.height+frame.uvScroll.y,v1=(s.y+s.height)/archive.height+frame.uvScroll.y;
    const Color color=Tint(frame.color,tint);
    BeginBlendMode(frame.additive ? BLEND_ADDITIVE : BLEND_ALPHA);
    rlSetTexture(texture.id);rlBegin(RL_QUADS);rlColor4ub(color.r,color.g,color.b,color.a);
    const std::array<Vector2,4> uv{{{u0,v0},{u1,v0},{u1,v1},{u0,v1}}};
    // 2D 的 Y 轴向下，必须反转绕序；镜像后再反转，不能依赖延迟批次的临时剔除状态。
    const std::array<int,4> indices=(screenSpace!=mirrored) ? std::array<int,4>{0,3,2,1} : std::array<int,4>{0,1,2,3};
    for(const int i:indices) {
        rlTexCoord2f(uv[i].x,uv[i].y);
        if(screenSpace)rlVertex2f(points[i].x,points[i].y);
        else rlVertex3f(points[i].x,points[i].y,points[i].z);
    }
    rlEnd();rlSetTexture(0);EndBlendMode();
}
}
std::array<Vector3,4> AnimatedBillboardVertices(const Camera3D& camera,const anm::Frame& frame,
                          Vector3 position,float height,Vector2 pivot,Vector3 motion) {
    if(frame.source.height<=0)return {position,position,position,position};
    const Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward));
    const float unit=height/frame.source.height;
    const float w=unit*frame.source.width*frame.scale.x,h=height*frame.scale.y;
    float angle=frame.rotation.z;
    if(frame.autoRotate && Vector3LengthSqr(motion)>1e-8F)
        angle+=std::atan2(-Vector3DotProduct(motion,up),Vector3DotProduct(motion,right))+PI/2;
    const Vector3 rotation{frame.rotation.x,frame.rotation.y,-angle};
    if(frame.topLeft)pivot={0,0};
    const std::array<Vector3,4> local{{{-w*pivot.x,h*pivot.y,0},{w*(1-pivot.x),h*pivot.y,0},
                                    {w*(1-pivot.x),-h*(1-pivot.y),0},{-w*pivot.x,-h*(1-pivot.y),0}}};
    std::array<Vector3,4> points{};
    for(int i=0;i<4;++i) {
        auto p=Rotate(local[i],rotation);
        const auto offset=Vector3Add(frame.position,frame.positionOffset);
        p=Vector3Add(p,{offset.x*unit,-offset.y*unit,offset.z*unit});
        points[i]=Vector3Add(position,Vector3Add(Vector3Scale(right,p.x),Vector3Add(Vector3Scale(up,p.y),Vector3Scale(forward,p.z))));
    }
    return points;
}
void DrawAnimatedBillboard(const Camera3D& camera,Texture2D texture,const anm::Archive& archive,
                          const anm::Frame& frame,Vector3 position,float height,Vector2 pivot,Color tint,Vector3 motion) {
    if(frame.source.height<=0)return;
    const auto points=AnimatedBillboardVertices(camera,frame,position,height,pivot,motion);
    // 镜像后的顶点绕序可能反转；公告板两面均可见，调用方统一恢复剔除状态。
    rlDisableBackfaceCulling();Quad(texture,archive,frame,points,tint);
}
void DrawAnimatedSprite2D(Texture2D texture,const anm::Archive& archive,const anm::Frame& frame,
                          Vector2 position,float scale,Color tint) {
    const float width=frame.source.width*scale*frame.scale.x,height=frame.source.height*scale*frame.scale.y;
    const float x=frame.topLeft ? 0 : -width/2,y=frame.topLeft ? 0 : -height/2;
    const std::array<Vector2,4> local{{{x,y},{x+width,y},{x+width,y+height},{x,y+height}}};
    std::array<Vector3,4> points{};
    const auto offset=Vector3Add(frame.position,frame.positionOffset);
    for(int i=0;i<4;++i) {
        const auto p=Rotate({local[i].x,local[i].y,0},frame.rotation);
        points[i]={position.x+offset.x*scale+p.x,position.y+offset.y*scale+p.y,0};
    }
    const float winding=(points[1].x-points[0].x)*(points[3].y-points[0].y)-(points[1].y-points[0].y)*(points[3].x-points[0].x);
    Quad(texture,archive,frame,points,tint,true,winding<0);
}
void DrawAnimatedBeam(const Camera3D& camera,Texture2D texture,const anm::Archive& archive,
                      const anm::Frame& frame,Vector3 origin,Vector3 end,float width,Color tint) {
    const auto axis=Vector3Normalize(Vector3Subtract(end,origin));
    Vector3 side=Vector3CrossProduct(axis,Vector3Subtract(camera.position,origin));
    if(Vector3LengthSqr(side)<1e-8F)side=Vector3CrossProduct(axis,camera.up);
    side=Vector3Scale(Vector3Normalize(side),width*0.5F);
    const std::array<Vector3,4> points{{Vector3Subtract(end,side),Vector3Add(end,side),
                                     Vector3Add(origin,side),Vector3Subtract(origin,side)}};
    rlDisableBackfaceCulling();Quad(texture,archive,frame,points,tint);
}
} // namespace koumakyou
