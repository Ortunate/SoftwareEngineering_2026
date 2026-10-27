// 本文件统一绘制角色、弹幕、道具及 Bomb 视觉精灵/光束，处理透明排序、光晕和调试图层。

#include "render/sprite_renderer.hpp"
#include "render/animated_sprite.hpp"
#include "render/bomb_geometry.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace koumakyou {
namespace {

// 恋之御札激光额外加宽以改善三维视角下的辨识度；只影响显示，不改变原生判定宽度。
constexpr float kMarisaLaserVisualWidthScale = 1.5F;

/// @brief 计算原生激光带的世界全宽；自机脚本缩放只改变外观，不影响碰撞半径。
float AnimatedBeamWidth(const Bullet& beam,const anm::Frame& frame) {
    const float nativeScale=beam.playerShotKind==PlayerShotKind::Laser
        ? std::abs(frame.scale.x)*kMarisaLaserVisualWidthScale : 1.0F;
    return beam.radius*beam.BeamVisualScale()*2*nativeScale;
}

/// @brief 负宽度表示保守的无界投影；接触边界也视为重叠，不能漏掉柔边或舍入后的一个像素。
bool MayOverlap(Rectangle a,Rectangle b) {
    return a.width<0 || b.width<0 ||
        (a.x<=b.x+b.width && b.x<=a.x+a.width && a.y<=b.y+b.height && b.y<=a.y+a.height);
}

/// @brief 单张精灵的贴图、世界高度与归一化锚点。
struct SpriteSpec {
    Texture2D texture;
    float worldHeight;
    Vector2 pivotUv;
    const anm::Archive* archive = nullptr;
    anm::Frame animation{};
    Vector3 motion{};
};

/// @brief 计算两个三维向量的差。
/// @param first 被减向量。
/// @param second 减去的向量。
/// @return first - second。
Vector3 Subtract(Vector3 first, Vector3 second) {
    return {first.x - second.x, first.y - second.y,
            first.z - second.z};
}

/// @brief 计算叉积以构造朝向相机的正交基。
/// @param first 第一个向量。
/// @param second 第二个向量。
/// @return 叉积向量。
Vector3 Cross(Vector3 first, Vector3 second) {
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

/// @brief 计算两点的距离平方，避免绘制判断使用平方根。
/// @param first 第一个位置。
/// @param second 第二个位置。
/// @return 距离平方。
float DistanceSquared(Vector3 first, Vector3 second) {
    const Vector3 difference = Subtract(first, second);
    return difference.x * difference.x +
           difference.y * difference.y +
           difference.z * difference.z;
}

/// @brief 归一化朝向向量，零长度时退化为正 Y 方向。
/// @param vector 输入向量。
/// @return 单位方向向量。
Vector3 Normalized(Vector3 vector) {
    const float lengthSquared = DistanceSquared(vector, {});
    if (lengthSquared < 0.000001F) return {0.0F, 1.0F, 0.0F};
    const float scale = 1.0F / std::sqrt(lengthSquared);
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

/// @brief 根据相机朝向计算公告板贴图的屏幕上方向。
/// @param camera 当前相机。
/// @return 与视线垂直的单位上向量。
Vector3 ScreenUp(const Camera3D& camera) {
    // 从相机基向量求屏幕上方向，仰俯视时贴图仍完整朝向镜头。
    const Vector3 forward = Normalized(
        Subtract(camera.target, camera.position));
    const Vector3 right = Normalized(Cross(forward, camera.up));
    return Normalized(Cross(right, forward));
}

/// @brief 将一张贴图按世界尺寸和锚点放在指定位置。
/// @param camera 当前相机。
/// @param screenUp 贴图的屏幕上方向。
/// @param spec 贴图、大小和锚点参数。
/// @param position 锚点对应的世界坐标。
/// @param tint 颜色与透明度调制值。
void DrawSprite(const Camera3D& camera, Vector3 screenUp,
                const SpriteSpec& spec, Vector3 position, Color tint = WHITE) {
    if (!IsTextureValid(spec.texture)) return;
    if(spec.archive) {
        DrawAnimatedBillboard(camera,spec.texture,*spec.archive,spec.animation,position,
                              spec.worldHeight,spec.pivotUv,tint,spec.motion);
        return;
    }
    const float width = spec.worldHeight *
                        static_cast<float>(spec.texture.width) /
                        static_cast<float>(spec.texture.height);
    const Vector2 size{width, spec.worldHeight};
    // 贴图锚点是判定中心在图片中的位置，而非图片外框的中心。
    const Vector2 origin{width * spec.pivotUv.x,
                         spec.worldHeight * (1.0F - spec.pivotUv.y)};
    DrawBillboardPro(camera, spec.texture,
                     {0.0F, 0.0F, static_cast<float>(spec.texture.width),
                      static_cast<float>(spec.texture.height)},
                     position, screenUp, size, origin, 0.0F, tint);
}

/// @brief 将 Stage 外观参数转为公告板绘制参数。
SpriteSpec Spec(const VisualResource& visual,float height,const anm::Frame* frame=nullptr,Vector3 motion={}) {
    SpriteSpec result{visual.texture,height,visual.pivot};
    if(visual.archive) {
        result.archive=visual.archive.get();result.motion=motion;
        if(frame && frame->sprite>=0)result.animation=*frame;
        else {
            anm::Player initial;
            initial.Start(*visual.archive,visual.animation.script,visual.animation.spriteOffset,visual.animation.initialSprite);
            result.animation=initial.Sample();
        }
    }
    return result;
}

const VisualResource& FindVisual(std::span<const VisualResource> visuals,
                                 VisualId id) {
    static const VisualResource empty{};
    return id < visuals.size() ? visuals[id] : empty;
}

/// @brief 用相机平面中的实心圆环提供入退场光效；不依赖额外纹理。
void DrawActorRing(const Camera3D& camera, Vector3 up, Vector3 position,
                   float radius, Color color) {
    const Vector3 right = Normalized(Cross(
        Normalized(Subtract(camera.target, camera.position)), up));
    const auto point = [&](float angle, float size) {
        const float x = std::cos(angle) * size;
        const float y = std::sin(angle) * size;
        return Vector3{position.x + right.x * x + up.x * y,
                       position.y + right.y * x + up.y * y,
                       position.z + right.z * x + up.z * y};
    };
    constexpr int segments = 48;
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0F * std::numbers::pi_v<float> * i / segments;
        const float b = 2.0F * std::numbers::pi_v<float> * (i + 1) / segments;
        const Vector3 outerA = point(a, radius), outerB = point(b, radius);
        const Vector3 innerA = point(a, radius * 0.92F), innerB = point(b, radius * 0.92F);
        DrawTriangle3D(outerA, outerB, innerA, color);
        DrawTriangle3D(innerA, outerB, innerB, color);
    }
}

/// @brief 在三维球面分布爆散粒子，远近观察均有体积；progress 为 [0,1]。
void DrawActorBurst(Vector3 position, float radius, float progress, Color color,
                    ActorKind kind) {
    const int count = kind == ActorKind::Boss ? 24 : 12;
    for (int i = 0; i < count; ++i) {
        const float y = 1.0F - 2.0F * (i + 0.5F) / count;
        const float horizontal = std::sqrt(1.0F - y * y);
        const float angle = i * 2.39996323F;
        const float distance = radius * (0.4F + progress * 4.0F);
        // 远景粒子只占几像素，低细分避免整池击破时放大几何开销。
        DrawSphereEx({position.x + horizontal * std::cos(angle) * distance,
                      position.y + y * distance,
                      position.z + horizontal * std::sin(angle) * distance},
                     radius * 0.10F * (1.0F - progress), 4, 6, color);
    }
    // 很短的中心闪光衔接受击，之后只剩扩散的环与粒子。
    if (progress < 0.2F)
        DrawSphereEx(position, radius * 0.6F * (1.0F - progress / 0.2F), 8, 12, color);
}

/// @brief 绘制旧几何激光的透明外壳或预警线框；分段内部不加端盖/圆环，避免接缝重复加色。
void DrawBeamShell(Vector3 start,Vector3 end,float radius,Color color,bool capStart,bool capEnd,bool wire=false) {
    const auto axis=Normalized(Subtract(end,start));
    const auto side=Normalized(Cross(axis,std::abs(axis.y)<0.9F ? Vector3{0,1,0} : Vector3{1,0,0}));
    const auto up=Cross(axis,side);
    const auto point=[&](Vector3 center,float angle) {
        const float x=radius*std::cos(angle),y=radius*std::sin(angle);
        return Vector3{center.x+side.x*x+up.x*y,center.y+side.y*x+up.y*y,center.z+side.z*x+up.z*y};
    };
    constexpr int sides=12;
    for(int i=0;i<sides;++i) {
        const float a=i*2*PI/sides,b=(i+1)*2*PI/sides;
        const auto s0=point(start,a),s1=point(start,b),e0=point(end,a),e1=point(end,b);
        if(wire) {
            DrawLine3D(s0,e0,color);
            if(capStart)DrawLine3D(s0,s1,color);
            if(capEnd)DrawLine3D(e0,e1,color);
        } else {
            DrawTriangle3D(s0,e0,s1,color);DrawTriangle3D(s1,e0,e1,color);
            if(capStart)DrawTriangle3D(start,s1,s0,color);
            if(capEnd)DrawTriangle3D(end,e0,e1,color);
        }
    }
}

}  // namespace

void SpriteRenderer::Draw(const Camera3D& camera, const BulletSystem& bullets,
                          std::span<const behavior::Actor*> enemies, const Boss* boss,
                          std::span<const PickupSprite> pickups,
                          std::span<const VisualResource> visuals,
                          const SpriteShaders& shaders,
                          const ActorEffectSystem& effects,std::span<const BombVolumeSprite> bombVolumes) const {
    // 旧清单仍可持有 cutout/core shader；所有透明贴图改用连续 Alpha，不能让裁剪阈值切断柔边。
    (void)shaders;
    using Kind=TransparentDraw::Kind;
    const Vector3 forward=Normalized(Subtract(camera.target,camera.position));
    const Vector3 screenUp=ScreenUp(camera);
    const Vector3 screenRight=Normalized(Cross(forward,screenUp));
    transparentDraws_.clear();translucentActors_.clear();beams_.clear();spritePlanes_.clear();bombActorDepths_.clear();
    beamSegmentCount_=0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if(bullet.IsBeam())beams_.push_back(&bullet);
    });
    const auto depth=[&](Vector3 position) {
        const auto relative=Subtract(position,camera.position);
        return relative.x*forward.x+relative.y*forward.y+relative.z*forward.z;
    };
    const auto projectBounds=[&](const std::array<Vector3,4>& points) {
        float left=INFINITY,right=-INFINITY,bottom=INFINITY,top=-INFINITY;
        for(const auto point:points) {
            const auto relative=Subtract(point,camera.position);
            const float z=depth(point);
            // 跨过相机平面的四边形可能覆盖整个屏幕；不能用背后顶点的透视除法排除切分。
            if(z<=1e-4F)return Rectangle{0,0,-1,-1};
            const float divisor=camera.projection==CAMERA_PERSPECTIVE ? z : 1.0F;
            const float x=(relative.x*screenRight.x+relative.y*screenRight.y+relative.z*screenRight.z)/divisor;
            const float y=(relative.x*screenUp.x+relative.y*screenUp.y+relative.z*screenUp.z)/divisor;
            left=std::min(left,x);right=std::max(right,x);bottom=std::min(bottom,y);top=std::max(top,y);
        }
        constexpr float margin=1e-5F;
        return Rectangle{left-margin,bottom-margin,right-left+2*margin,top-bottom+2*margin};
    };
    const auto spritePlane=[&](const VisualResource& visual,float height,Vector3 position,
                               const anm::Frame* animation,Vector3 motion=Vector3{}) {
        const auto spec=Spec(visual,height,animation,motion);
        // 旧 PNG/几何外观保留既有切分；原生公告板使用与绘制完全相同的 ANM 四角，涵盖旋转、镜像和偏移。
        if(!spec.archive)return SpritePlane{depth(position)};
        const auto points=AnimatedBillboardVertices(camera,spec.animation,position,height,spec.pivotUv,motion);
        Vector3 center{};
        float near=INFINITY,far=-INFINITY;
        for(const auto point:points) {
            center.x+=point.x/4;center.y+=point.y/4;center.z+=point.z/4;
            if(!beams_.empty()) {const float z=depth(point);near=std::min(near,z);far=std::max(far,z);}
        }
        return SpritePlane{depth(center),beams_.empty() ? Rectangle{0,0,-1,-1} : projectBounds(points),near,far};
    };
    glowCounts_.assign(visuals.size(),0);
    const auto queue=[&](TransparentDraw command,SpritePlane plane) {
        command.depth=plane.depth;
        if(!beams_.empty())spritePlanes_.push_back(plane);
        if(command.kind==Kind::Actor)bombActorDepths_.push_back(plane.depth);
        transparentDraws_.push_back(command);
    };
    const auto addActor=[&](Vector3 position,VisualId id,float radius,ActorKind kind,
                            ActorAppearance appearance,const anm::Frame& animation) {
        const float nearOpacity=std::clamp((std::sqrt(DistanceSquared(position,camera.position))-0.6F)/1.2F,0.0F,1.0F);
        appearance.alpha*=nearOpacity;
        const auto index=translucentActors_.size();
        translucentActors_.push_back({position,id,radius,kind,appearance,animation});
        const auto& visual=FindVisual(visuals,id);
        queue({Kind::Actor,0,index},spritePlane(visual,visual.worldHeight*appearance.scale,position,&animation));
    };
    for(const auto* enemy:enemies)
        addActor(enemy->position,enemy->visual,enemy->hitRadius,ActorKind::Enemy,
                 enemy->presentation.Sample(),enemy->animation.player.Sample());
    if(boss && boss->active)
        addActor(boss->position,boss->visual,boss->hitRadius,ActorKind::Boss,
                 boss->presentation.Sample(),boss->animation.player.Sample());
    effects.Pool().ForEachActive([&](const ActorExitEffect& effect) {
        addActor(effect.position,effect.visual,effect.hitRadius,effect.kind,effect.Sample(),effect.animation);
    });
    for(std::size_t index=0;index<pickups.size();++index) {
        const auto& pickup=pickups[index];const auto& visual=FindVisual(visuals,pickup.visual);
        queue({Kind::Pickup,0,index},spritePlane(visual,visual.worldHeight*pickup.heightScale,pickup.position,pickup.animation));
    }

    // 只让真实不透明体积写入深度。贴图即使 Alpha=0 也会写矩形深度，所以不能放在此通道。
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        const auto& visual=FindVisual(visuals,bullet.visual);
        if(bullet.IsBeam()) {
            if(!visual.archive && (bullet.Dangerous() || bullet.beamGrowSeconds>0)) {
                const auto end=bullet.BeamEnd();
                const float width=bullet.radius*bullet.BeamVisualScale();
                DrawCylinderEx(bullet.position,end,width*0.45F,width*0.45F,10,WHITE);
            }
            return;
        }
        if(visual.mode==VisualMode::Sphere && bullet.color.a==255) {
            DrawSphere(bullet.position,bullet.radius,bullet.color);return;
        }
        const float distance=DistanceSquared(bullet.position,camera.position);
        if(bullet.visual>=visuals.size() || distance<0.7F*0.7F)return;
        if(!visual.archive && bullet.owner==BulletOwner::Enemy && distance<1.1F*1.1F)return;
        const auto plane=spritePlane(visual,bullet.radius*visual.bulletDiameterScale,bullet.position,
                                  &bullet.animation.player.Sample(),bullet.velocity);
        // 同深度按插入顺序稳定绘制，光晕先于自身弹体，避免光效盖住前方角色或道具。
        if(visual.glow && !visual.archive && distance<=8.0F*8.0F && glowCounts_[bullet.visual]++<2048)
            queue({Kind::Glow,0,0,&bullet},plane);
        queue({Kind::Bullet,0,0,&bullet},plane);
    });

    if(!beams_.empty() || !bombVolumes.empty()) {
        std::sort(spritePlanes_.begin(),spritePlanes_.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
        std::sort(bombActorDepths_.begin(),bombActorDepths_.end());
        bombActorDepths_.erase(std::unique(bombActorDepths_.begin(),bombActorDepths_.end()),bombActorDepths_.end());
    }
    const auto queueBeam=[&](Vector3 origin,Vector3 end,Kind kind,std::size_t index,const Bullet* beam,
                             Rectangle bounds=Rectangle{0,0,-1,-1},const std::array<Vector3,4>* points=nullptr) {
        const float first=depth(origin),last=depth(end),difference=last-first;
        beamCuts_={0,1};
        // 立体巨束自身也需要轴向分段，侧视和沿光轴观看时均按真实前后顺序混合三个光壳。
        if(kind==Kind::BombVolume)for(int i=1;i<12;++i)beamCuts_.push_back(i/12.0F);
        // 一条光束可能同时在演员前后：在精灵平面处切开，UV 仍沿整束连续，不能整束按中点排序。
        if(std::abs(difference)>1e-5F) {
            const auto cut=[&](float d) {
                const float t=(d-first)/difference;
                if(t>1e-5F && t<1-1e-5F)beamCuts_.push_back(t);
            };
            if(kind==Kind::BombVolume) {
                // 巨束仍只按演员精确切分，小弹/道具沿既有轴向区间排序；判定不受渲染分段数影响。
                const auto begin=std::upper_bound(bombActorDepths_.begin(),bombActorDepths_.end(),std::min(first,last));
                const auto finish=std::lower_bound(bombActorDepths_.begin(),bombActorDepths_.end(),std::max(first,last));
                for(auto it=begin;it!=finish;++it)cut(*it);
            } else {
                const auto begin=std::upper_bound(spritePlanes_.begin(),spritePlanes_.end(),std::min(first,last),
                                                  [](float d,const auto& plane){return d<plane.depth;});
                const auto finish=std::lower_bound(spritePlanes_.begin(),spritePlanes_.end(),std::max(first,last),
                                                   [](const auto& plane,float d){return plane.depth<d;});
                const float sideDepth=points ? std::abs(depth((*points)[1])-depth((*points)[0]))*0.5F : 0;
                for(auto it=begin;it!=finish;++it) {
                    if(!MayOverlap(bounds,it->bounds))continue;
                    if(points && bounds.width>=0 && it->bounds.width>=0) {
                        // 整束投影在透视下可能包住很大空白；只检查精灵真实深度区间内的光带。
                        // sideDepth 覆盖光带侧向的深度偏差，四角区间覆盖 ANM 倾斜，不能退化成中心点测试。
                        const float a=(it->nearDepth-sideDepth-1e-4F-first)/difference;
                        const float b=(it->farDepth+sideDepth+1e-4F-first)/difference;
                        const float from=std::clamp(std::min(a,b),0.0F,1.0F),to=std::clamp(std::max(a,b),0.0F,1.0F);
                        const auto at=[&](int corner,float t) {
                            const auto start=(*points)[corner],finish=(*points)[corner==3 ? 0 : 1];
                            return Vector3{start.x+(finish.x-start.x)*t,start.y+(finish.y-start.y)*t,start.z+(finish.z-start.z)*t};
                        };
                        const std::array<Vector3,4> slice{{at(3,to),at(2,to),at(2,from),at(3,from)}};
                        if(!MayOverlap(projectBounds(slice),it->bounds))continue;
                    }
                    cut(it->depth);
                }
            }
        }
        std::sort(beamCuts_.begin(),beamCuts_.end());
        beamCuts_.erase(std::unique(beamCuts_.begin(),beamCuts_.end()),beamCuts_.end());
        if(kind==Kind::Beam)beamSegmentCount_+=beamCuts_.size()-1;
        for(std::size_t i=1;i<beamCuts_.size();++i) {
            const float from=beamCuts_[i-1],to=beamCuts_[i];
            transparentDraws_.push_back({kind,first+(from+to)*0.5F*difference,index,beam,from,to});
        }
    };
    for(const Bullet* beam:beams_) {
        const auto& visual=FindVisual(visuals,beam->visual);
        Rectangle bounds{0,0,-1,-1};
        std::array<Vector3,4> points{};
        if(visual.archive) {
            const auto frame=Spec(visual,1,&beam->animation.player.Sample()).animation;
            points=AnimatedBeamVertices(camera,beam->position,beam->BeamEnd(),AnimatedBeamWidth(*beam,frame));
            bounds=projectBounds(points);
        }
        queueBeam(beam->position,beam->BeamEnd(),Kind::Beam,0,beam,bounds,visual.archive ? &points : nullptr);
    }
    for(std::size_t i=0;i<bombVolumes.size();++i) {
        const auto* sprite=bombVolumes[i].sprite;
        if(!sprite)continue;
        if(sprite->shape==PlayerBombSprite::Shape::Beam)
            queueBeam(sprite->volume.origin,sprite->volume.end,Kind::BombVolume,i,nullptr);
        else transparentDraws_.push_back({Kind::BombVolume,depth(sprite->volume.origin),i});
    }
    // 沿视线投影的深度才决定遮挡；到相机的欧氏距离会在斜视/大公告板重叠时反转顺序。
    std::stable_sort(transparentDraws_.begin(),transparentDraws_.end(),
                     [](const auto& left,const auto& right){return left.depth>right.depth;});
    rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ALPHA);
    for(const auto& command:transparentDraws_) {
        // 原生激光保留当前混合模式，相邻加色片段可合批；其他绘制入口仍以 Alpha 为默认状态。
        if(command.kind!=Kind::Beam)BeginBlendMode(BLEND_ALPHA);
        if(command.kind==Kind::Actor) {
            const auto& actor=translucentActors_[command.index];const auto& appearance=actor.appearance;
            const auto& visual=FindVisual(visuals,actor.visual);
            const auto opacity=static_cast<unsigned char>(appearance.alpha*255);
            if(opacity!=0) {
                if(IsTextureValid(visual.texture))
                    DrawSprite(camera,screenUp,Spec(visual,visual.worldHeight*appearance.scale,&actor.animation),
                               actor.position,{255,255,255,opacity});
                else DrawSphere(actor.position,actor.hitRadius*appearance.scale,Fade(SKYBLUE,appearance.alpha));
            }
            const Color color=appearance.burstAlpha>0 ? GOLD : actor.kind==ActorKind::Boss
                ? Color{230,140,255,255} : Color{150,215,255,255};
            if(!visual.archive && appearance.ringAlpha>0.001F)
                DrawActorRing(camera,screenUp,actor.position,actor.hitRadius*appearance.ringRadius,Fade(color,appearance.ringAlpha));
            if(!visual.archive && appearance.burstAlpha>0.001F)
                DrawActorBurst(actor.position,actor.hitRadius,appearance.burst,Fade(color,appearance.burstAlpha),actor.kind);
        } else if(command.kind==Kind::Pickup) {
            const auto& pickup=pickups[command.index];const auto& visual=FindVisual(visuals,pickup.visual);
            const float height=visual.worldHeight*pickup.heightScale;
            if(IsTextureValid(visual.texture))DrawSprite(camera,screenUp,Spec(visual,height,pickup.animation),
                                                       pickup.position,{255,255,255,pickup.alpha});
            else DrawSphere(pickup.position,height*0.35F,Fade(RED,pickup.alpha/255.0F));
        } else if(command.kind==Kind::Bullet) {
            const auto& bullet=*command.bullet;const auto& visual=visuals[bullet.visual];
            if(visual.mode==VisualMode::Sphere)DrawSphere(bullet.position,bullet.radius,bullet.color);
            else DrawSprite(camera,screenUp,Spec(visual,bullet.radius*visual.bulletDiameterScale,
                            &bullet.animation.player.Sample(),bullet.velocity),bullet.position,{255,255,255,bullet.color.a});
        } else if(command.kind==Kind::Glow) {
            const auto& bullet=*command.bullet;const auto& visual=visuals[bullet.visual];
            const float distance=std::sqrt(DistanceSquared(bullet.position,camera.position));
            const float nearFade=std::clamp((distance-0.35F)/0.75F,0.0F,1.0F);
            const auto opacity=static_cast<unsigned char>(36+28*nearFade);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSprite(camera,screenUp,Spec(visual,bullet.radius*(distance<1.1F ? 2.6F : 3.15F)),
                       bullet.position,{255,255,255,opacity});
            EndBlendMode();
        } else if(command.kind==Kind::BombVolume) {
            const auto& volume=bombVolumes[command.index];const auto& visual=FindVisual(visuals,volume.visual);
            if(visual.archive && volume.sprite) {
                auto sample=*volume.sprite;
                bool interiorBeam=false;
                if(sample.shape==PlayerBombSprite::Shape::Beam) {
                    const auto axis=Normalized(Subtract(sample.volume.end,sample.volume.origin));
                    interiorBeam=DistanceSquared(camera.position,sample.volume.origin)<0.8F*0.8F &&
                                 forward.x*axis.x+forward.y*axis.y+forward.z*axis.z>0.9F;
                    // 从束内观察改画有视差的流光而非圆台外壁，曝光由绘制器统一控制；外部仍显示完整巨束。
                }
                if(sample.shape==PlayerBombSprite::Shape::Sphere || sample.shape==PlayerBombSprite::Shape::Star) {
                    // 镜头进入球体时保留实际几何和攻击，仅减弱近眼曝光，防止回旋光球把第一人称画面染成整屏白块。
                    const float distance=std::sqrt(DistanceSquared(camera.position,sample.volume.origin));
                    const float visualRadius=sample.height*0.5F*std::max(std::abs(sample.frame.scale.y),
                        std::abs(sample.frame.scale.x)*sample.frame.source.width/std::max(1.0F,sample.frame.source.height));
                    const float radius=std::max({0.01F,sample.volume.radius,visualRadius});
                    const float near=std::clamp((distance/radius-0.35F)/1.3F,0.0F,1.0F);
                    const float minimum=sample.shape==PlayerBombSprite::Shape::Star ? 0.005F : 0.075F;
                    sample.frame.color.a=static_cast<unsigned char>(sample.frame.color.a*std::max(minimum,near*near*near*near));
                }
                DrawBombGeometry(visual.texture,*visual.archive,sample,command.from,command.to,interiorBeam);
            }
        } else {
            const auto& beam=*command.bullet;const auto& visual=FindVisual(visuals,beam.visual);
            const auto point=[&](float t) {return Vector3{beam.position.x+beam.beamDirection.x*beam.beamLength*t,
                beam.position.y+beam.beamDirection.y*beam.beamLength*t,beam.position.z+beam.beamDirection.z*beam.beamLength*t};};
            const auto start=point(command.from),end=point(command.to);
            const float width=beam.radius*beam.BeamVisualScale();
            if(visual.archive) {
                auto frame=Spec(visual,1,&beam.animation.player.Sample()).animation;
                frame.source.y+=(1-command.to)*frame.source.height;
                frame.source.height*=command.to-command.from;
                // 自机激光三级原生脚本以 scale.x 控制横向宽度，不能只换颜色而丢失强度档位的形状。
                DrawAnimatedBeam(camera,visual.texture,*visual.archive,frame,start,end,AnimatedBeamWidth(beam,frame),{255,255,255,beam.color.a});
            } else if(!beam.Dangerous() && beam.beamGrowSeconds<=0) {
                BeginBlendMode(BLEND_ALPHA);
                DrawLine3D(start,end,Fade(beam.color,0.7F));
                DrawBeamShell(start,end,beam.radius*0.3F,Fade(beam.color,0.4F),command.from==0,command.to==1,true);
            } else {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawBeamShell(start,end,width,Fade(beam.color,0.7F),command.from==0,command.to==1);
                DrawBeamShell(start,end,width*1.5F,Fade(beam.color,0.2F),command.from==0,command.to==1);
                EndBlendMode();
            }
        }
    }
    EndBlendMode();
    // rlgl 批次延迟提交，必须先 flush 再恢复深度写入/剔除，否则最后一批透明矩形会污染深度。
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
}

#ifdef KOUMAKYOU_DEBUG_FEATURES
void SpriteRenderer::DrawDebugGeometry(const BulletSystem& bullets,
                                       std::span<const behavior::Actor*> enemies,
                                       const Boss* boss,
                                       Vector3 playerPosition) const {
    rlDisableDepthTest();
    rlDisableDepthMask();
    int shownBullets = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (shownBullets++ >= 256) return;
        if (bullet.IsBeam()) {
            DrawCylinderWiresEx(bullet.position, bullet.BeamEnd(), bullet.radius, bullet.radius,
                                10, Color{255, 230, 90, 230});
            return;
        }
        DrawSphereWires(bullet.position, bullet.radius, 7, 10,
                        Color{255, 230, 90, 230});
    });
    for (const behavior::Actor* enemy : enemies) {
        DrawSphereWires(enemy->position, enemy->hitRadius, 9, 12,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = enemy->MuzzlePosition();
        DrawLine3D(enemy->position, muzzle, YELLOW);
        DrawSphere(muzzle, 0.065F, MAGENTA);
    }
    if (boss && boss->active) {
        DrawSphereWires(boss->position, boss->hitRadius, 12, 16,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = boss->MuzzlePosition(playerPosition);
        DrawLine3D(boss->position, muzzle, YELLOW);
        DrawSphere(muzzle, 0.09F, MAGENTA);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
}
#endif

}  // namespace koumakyou
