// 本文件把 Bomb 绘制为实体星、球形光晕、六面空间符阵及圆截面光束；所有顶点均为世界坐标。
#include "render/bomb_geometry.hpp"
#include "app/math.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
namespace koumakyou {
namespace {
using namespace game_math;
Vector3 Cross(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct Axes { Vector3 right,up; };
Axes BeamAxes(const BombVolume& volume) {
    const auto axis=Normalize(Subtract(volume.end,volume.origin));
    const auto right=Normalize(Cross(axis,std::abs(axis.y)<0.9F ? Vector3{0,1,0} : Vector3{1,0,0}));
    return {right,Normalize(Cross(right,axis))};
}
Color Alpha(Color color,float multiplier) { color.a=static_cast<unsigned char>(std::clamp(color.a*multiplier,0.0F,255.0F));return color; }
/// @brief UV 为原精灵内的归一化坐标；调用方已开启加色混合，不在每个四边形反复切换批次。
void Quad(Texture2D texture,const anm::Archive& archive,const anm::Frame& frame,
          const std::array<Vector3,4>& points,Color color,const std::array<Vector2,4>& uv={{{0,0},{1,0},{1,1},{0,1}}}) {
    // rlBegin 切换图元类型会重置批次纹理；必须先切模式再绑定，否则实体三角面后的第一张光晕会变白块。
    rlBegin(RL_QUADS);rlSetTexture(texture.id);rlColor4ub(color.r,color.g,color.b,color.a);
    for(int i=0;i<4;++i) {
        rlTexCoord2f((frame.source.x+uv[i].x*frame.source.width)/archive.width+frame.uvScroll.x,
                     (frame.source.y+uv[i].y*frame.source.height)/archive.height+frame.uvScroll.y);
        rlVertex3f(points[i].x,points[i].y,points[i].z);
    }
    rlEnd();rlSetTexture(0);
}
void Plane(Texture2D texture,const anm::Archive& archive,const anm::Frame& frame,
           Vector3 center,Vector3 right,Vector3 up,float width,float height,Color color,float rotation=0) {
    const auto r=Add(Scale(right,std::cos(rotation)),Scale(up,std::sin(rotation)));
    const auto u=Subtract(Scale(up,std::cos(rotation)),Scale(right,std::sin(rotation)));
    const auto x=Scale(r,width/2),y=Scale(u,height/2);
    Quad(texture,archive,frame,{Add(center,Subtract(y,x)),Add(center,Add(x,y)),Add(center,Subtract(x,y)),Subtract(center,Add(x,y))},color);
}
void Ring(Vector3 center,Vector3 right,Vector3 up,float radiusX,float radiusY,Color color,float rotation=0) {
    const auto point=[&](float a) {return Add(center,Add(Scale(right,std::cos(a+rotation)*radiusX),Scale(up,std::sin(a+rotation)*radiusY)));};
    for(int i=0;i<64;++i)DrawLine3D(point(i*2*PI/64),point((i+1)*2*PI/64),color);
}
void DrawSeal(Texture2D texture,const anm::Archive& archive,const PlayerBombSprite& sprite) {
    const auto& v=sprite.volume;const auto color=sprite.frame.color;
    int axis=0;
    if(v.halfExtents.y<v.halfExtents.x)axis=1;
    if(v.halfExtents.z<(axis==0 ? v.halfExtents.x : v.halfExtents.y))axis=2;
    const Vector3 normal=axis==0 ? Vector3{1,0,0} : axis==1 ? Vector3{0,1,0} : Vector3{0,0,1};
    const Vector3 right=axis==0 ? Vector3{0,0,1} : Vector3{1,0,0};
    const Vector3 up=axis==1 ? Vector3{0,0,1} : Vector3{0,1,0};
    const float w=axis==0 ? v.halfExtents.z : v.halfExtents.x;
    const float h=axis==1 ? v.halfExtents.z : v.halfExtents.y;
    const float thickness=axis==0 ? v.halfExtents.x : axis==1 ? v.halfExtents.y : v.halfExtents.z;
    // 三层固定平面给符阵可见厚度；外层就是伤害薄盒的前后表面，侧视和转头均不会重新朝向镜头。
    for(float side:{-1.0F,0.0F,1.0F}) {
        const auto center=Add(v.origin,Scale(normal,side*thickness));
        Plane(texture,archive,sprite.frame,center,right,up,w*2,h*2,Alpha(color,side==0 ? 0.28F : 0.12F));
        Ring(center,right,up,w*0.94F,h*0.94F,Alpha(color,0.75F));
        Ring(center,right,up,w*0.75F,h*0.75F,Alpha(color,0.45F));
        for(int i=0;i<12;++i) {
            const float a=i*2*PI/12+sprite.ageSeconds*0.25F;
            const auto p=Add(center,Add(Scale(right,std::cos(a)*w*0.9F),Scale(up,std::sin(a)*h*0.9F)));
            const auto q=Add(center,Add(Scale(right,std::cos(a)*w*0.77F),Scale(up,std::sin(a)*h*0.77F)));
            DrawLine3D(p,q,Alpha(color,0.8F));
        }
    }
    // 网格覆盖整片截面，明确显示场地大小的扫场判定，而不是悬浮在准星前的小十字贴图。
    for(float x=-w;x<=w;x+=2)
        DrawLine3D(Add(v.origin,Add(Scale(right,x),Scale(up,-h))),Add(v.origin,Add(Scale(right,x),Scale(up,h))),Alpha(color,0.2F));
    for(float y=-h;y<=h;y+=2)
        DrawLine3D(Add(v.origin,Add(Scale(right,-w),Scale(up,y))),Add(v.origin,Add(Scale(right,w),Scale(up,y))),Alpha(color,0.2F));
    const float r=std::min(w,h)*0.62F;
    for(int i=0;i<6;++i) {
        const float a=i*PI/3+sprite.ageSeconds*0.25F,b=a+2*PI/3;
        DrawLine3D(Add(v.origin,Add(Scale(right,std::cos(a)*r),Scale(up,std::sin(a)*r))),
                   Add(v.origin,Add(Scale(right,std::cos(b)*r),Scale(up,std::sin(b)*r))),Alpha(color,0.7F));
    }
}
void DrawOrbOrStar(Texture2D texture,const anm::Archive& archive,const PlayerBombSprite& sprite) {
    const auto color=sprite.frame.color;
    const bool star=sprite.shape==PlayerBombSprite::Shape::Star;
    if(!star)DrawSphereEx(sprite.position,sprite.volume.radius,10,16,Alpha(color,0.08F));
    else if(!sprite.trail) {
        // 六个实心锥形星芒具有 XYZ 方向的真实厚度，三张原生贴图只作为包覆光晕。
        const float r=sprite.height*0.42F;
        for(const auto axis:std::array<Vector3,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}}) {
            const auto side=Normalize(Cross(axis,std::abs(axis.y)<0.9F ? Vector3{0,1,0} : Vector3{1,0,0}));
            const auto up=Cross(axis,side),tip=Add(sprite.position,Scale(axis,r));
            for(int i=0;i<4;++i) {
                const auto point=[&](float a) {return Add(sprite.position,Add(Scale(side,std::cos(a)*r*0.18F),Scale(up,std::sin(a)*r*0.18F)));};
                DrawTriangle3D(tip,point(i*PI/2),point((i+1)*PI/2),Alpha(color,0.5F));
            }
        }
    }
    const float h=sprite.height*std::abs(sprite.frame.scale.y);
    const float w=sprite.height*sprite.frame.LogicalSize().x/std::max(1.0F,sprite.frame.LogicalSize().y)*std::abs(sprite.frame.scale.x);
    for(const auto axes:std::array<Axes,3>{{{{1,0,0},{0,1,0}},{{0,0,1},{0,1,0}},{{1,0,0},{0,0,1}}}})
        Plane(texture,archive,sprite.frame,sprite.position,axes.right,axes.up,w,h,Alpha(color,0.65F),sprite.frame.rotation.z);
}
/// @brief 沿真实束轴绘制内部流光，范围限于当前透明排序切片；不画封闭截面或更改攻击形状。
void DrawBeamInterior(const PlayerBombSprite& sprite,float from,float to) {
    if(sprite.layer!=0)return; // 四层 ANM 共用一次内部表现，防止颜色叠成过曝的白屏。
    const auto& volume=sprite.volume;const auto axes=BeamAxes(volume);
    const auto offset=Subtract(volume.end,volume.origin);const float length=BulletSystem::Length(offset);
    from=std::max(from,0.8F/length);
    if(from>=to)return;
    constexpr int strands=12,steps=3;
    constexpr std::array<Color,4> colors{{{80,205,255,255},{255,110,210,255},{255,210,90,255},{175,125,255,255}}};
    // 光线从周边掠过并向远处汇聚，中央不画盘面。所有点都在世界束轴基底内，侧移/转向具有真实视差。
    const auto sample=[&](float t,int strand,float width) {
        const float distance=t*length,phase=strand*2*PI/strands;
        const float angle=phase+0.035F*std::sin(distance*0.35F-sprite.ageSeconds*3+phase);
        const float radius=std::min(volume.radius*0.8F,1.65F+distance*0.075F);
        const auto radial=Add(Scale(axes.right,std::cos(angle)),Scale(axes.up,std::sin(angle)));
        const auto tangent=Add(Scale(axes.right,-std::sin(angle)),Scale(axes.up,std::cos(angle)));
        return Add(volume.origin,Add(Scale(offset,t),Add(Scale(radial,radius),Scale(tangent,width*radius))));
    };
    const auto strength=[&](float t,int strand) {
        const float distance=t*length;
        const float ends=std::clamp((distance-0.8F)/2,0.0F,1.0F)*std::clamp((0.92F-t)/0.35F,0.0F,1.0F);
        const float pulse=0.5F+0.5F*std::sin(distance*0.85F-sprite.ageSeconds*12+strand*1.7F);
        // ANM 为四层叠加设计；束内只画一层，重新标定曝光并继续遵守其淡出 Alpha。
        return ends*(0.45F+0.55F*pulse)*std::min(1.0F,sprite.frame.color.a/96.0F);
    };
    rlSetTexture(0);rlBegin(RL_QUADS);
    for(int strand=0;strand<strands;++strand)for(int step=0;step<steps;++step) {
        const float a=from+(to-from)*step/steps,b=from+(to-from)*(step+1)/steps;
        // 外晕比白色细芯更宽更淡；沿光程渐变而非重复铺圆环，切片数变化也不叠加亮度。
        for(int glow=0;glow<3;++glow) {
            const float width=glow==0 ? 0.20F : glow==1 ? 0.045F : 0.009F;
            const auto base=glow==2 ? Color{221,244,255,255} : colors[strand%4];
            const float opacity=glow==0 ? 0.22F : glow==1 ? 0.55F : 0.9F;
            // 两个半带的中央亮、外缘透明，避免硬边矩形；宽光晕和窄芯共享相同的连续中心线。
            for(float side:{-1.0F,1.0F})for(const auto vertex:std::array<Vector2,4>{{{a,0},{a,side*width},{b,side*width},{b,0}}}) {
                const auto color=Alpha(base,strength(vertex.x,strand)*opacity*(vertex.y==0 ? 1 : 0));
                const auto p=sample(vertex.x,strand,vertex.y);
                rlColor4ub(color.r,color.g,color.b,color.a);rlVertex3f(p.x,p.y,p.z);
            }
        }
    }
    rlEnd();
}
void DrawBeam(Texture2D texture,const anm::Archive& archive,const PlayerBombSprite& sprite,float from,float to,bool interior) {
    if(interior) { DrawBeamInterior(sprite,from,to);return; }
    constexpr int sides=16;
    const auto& volume=sprite.volume;
    const auto axes=BeamAxes(volume);
    const auto offset=Subtract(volume.end,volume.origin);
    if(from>=to)return;
    const auto point=[&](float t,float angle,float shell=1.0F) {
        // 攻击外缘是宽束，能量密度较高的内芯在出射后四单位内张开，外部观察能读出角色位置的发射源。
        const float flare=shell<1 ? std::clamp(t*BulletSystem::Length(offset)/4,0.012F,1.0F) : 1;
        const float radius=(volume.radius+(volume.endRadius-volume.radius)*t)*shell*flare;
        return Add(volume.origin,Add(Scale(offset,t),Add(Scale(axes.right,std::cos(angle)*radius),Scale(axes.up,std::sin(angle)*radius))));
    };
    // 三个不同半径的实体光壳，外壳与攻击圆台一致。没有面向相机的带子，也没有遮住主视角的近端封盖。
    for(float shell:{1.0F,0.72F,0.4F}) {
        const auto color=Alpha(sprite.frame.color,shell==1 ? 0.12F : 0.6F);
        for(int i=0;i<sides;++i) {
            const float a=i*2*PI/sides,b=(i+1)*2*PI/sides;
            const float u0=0.5F+std::cos(a)*0.17F,u1=0.5F+std::cos(b)*0.17F;
            Quad(texture,archive,sprite.frame,{point(to,a,shell),point(to,b,shell),point(from,b,shell),point(from,a,shell)},color,
                {{{u0,1-to},{u1,1-to},{u1,1-from},{u0,1-from}}});
        }
    }
    // 在实体内部按光程积分发光密度。每段亮度乘实际长度，透明排序额外切片不能把亮度重复乘倍。
    // 圆盘垂直于光束轴而非相机；正面可见发光核心，侧面仍由三层真实圆台壳提供连续轮廓。
    const float t=(from+to)/2;
    const auto center=Add(volume.origin,Scale(offset,t));
    const auto core=Alpha(sprite.frame.color,0.45F*(to-from)*24);
    const auto edge=Alpha(sprite.frame.color,0);
    rlSetTexture(0);rlBegin(RL_TRIANGLES);
    for(int i=0;i<sides;++i) {
        const auto a=point(t,i*2*PI/sides,0.72F),b=point(t,(i+1)*2*PI/sides,0.72F);
        rlColor4ub(core.r,core.g,core.b,core.a);rlVertex3f(center.x,center.y,center.z);
        rlColor4ub(edge.r,edge.g,edge.b,edge.a);rlVertex3f(a.x,a.y,a.z);rlVertex3f(b.x,b.y,b.z);
    }
    rlEnd();
}
}
Vector3 BombBeamSurface(const BombVolume& volume,float t,float angle,float shellScale) {
    const auto axes=BeamAxes(volume);const float radius=(volume.radius+(volume.endRadius-volume.radius)*t)*shellScale;
    return Add(volume.origin,Add(Scale(Subtract(volume.end,volume.origin),t),
           Add(Scale(axes.right,std::cos(angle)*radius),Scale(axes.up,std::sin(angle)*radius))));
}
void DrawBombGeometry(Texture2D texture,const anm::Archive& archive,const PlayerBombSprite& sprite,float from,float to,bool interiorBeam) {
    if(!IsTextureValid(texture) || !sprite.frame.visible || sprite.frame.color.a==0)return;
    rlDisableBackfaceCulling();BeginBlendMode(BLEND_ADDITIVE);
    if(sprite.shape==PlayerBombSprite::Shape::Beam)DrawBeam(texture,archive,sprite,from,to,interiorBeam);
    else if(sprite.shape==PlayerBombSprite::Shape::Slab)DrawSeal(texture,archive,sprite);
    else DrawOrbOrStar(texture,archive,sprite);
    EndBlendMode();
}
} // namespace koumakyou
