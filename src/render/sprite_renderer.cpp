// 本文件负责实现敌人、Boss、弹幕与道具的公告板绘制、光晕及调试图层。

#include "render/sprite_renderer.hpp"
#include "render/animated_sprite.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace koumakyou {
namespace {

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
                          std::span<const Enemy*> enemies, const Boss* boss,
                          std::span<const PickupSprite> pickups,
                          std::span<const VisualResource> visuals,
                          const SpriteShaders& shaders,
                          const ActorEffectSystem& effects) const {
    // 旧清单仍可持有 cutout/core shader；所有透明贴图改用连续 Alpha，不能让裁剪阈值切断柔边。
    (void)shaders;
    using Kind=TransparentDraw::Kind;
    const Vector3 forward=Normalized(Subtract(camera.target,camera.position));
    const Vector3 screenUp=ScreenUp(camera);
    const auto depth=[&](Vector3 position) {
        const auto relative=Subtract(position,camera.position);
        return relative.x*forward.x+relative.y*forward.y+relative.z*forward.z;
    };
    const auto spriteDepth=[&](const VisualResource& visual,float height,Vector3 position,
                               const anm::Frame* animation,Vector3 motion=Vector3{}) {
        const auto spec=Spec(visual,height,animation,motion);
        if(!spec.archive)return depth(position);
        const auto points=AnimatedBillboardVertices(camera,spec.animation,position,height,spec.pivotUv,motion);
        Vector3 center{};
        for(const auto point:points) {center.x+=point.x/4;center.y+=point.y/4;center.z+=point.z/4;}
        return depth(center);
    };
    transparentDraws_.clear();translucentActors_.clear();beams_.clear();spriteDepths_.clear();
    glowCounts_.assign(visuals.size(),0);
    const auto queue=[&](TransparentDraw command) {
        spriteDepths_.push_back(command.depth);transparentDraws_.push_back(command);
    };
    const auto addActor=[&](Vector3 position,VisualId id,float radius,ActorKind kind,
                            ActorAppearance appearance,const anm::Frame& animation) {
        const float nearOpacity=std::clamp((std::sqrt(DistanceSquared(position,camera.position))-0.6F)/1.2F,0.0F,1.0F);
        appearance.alpha*=nearOpacity;
        const auto index=translucentActors_.size();
        translucentActors_.push_back({position,id,radius,kind,appearance,animation});
        const auto& visual=FindVisual(visuals,id);
        queue({Kind::Actor,spriteDepth(visual,visual.worldHeight*appearance.scale,position,&animation),index});
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
        queue({Kind::Pickup,spriteDepth(visual,visual.worldHeight*pickup.heightScale,pickup.position,pickup.animation),index});
    }

    // 只让真实不透明体积写入深度。贴图即使 Alpha=0 也会写矩形深度，所以不能放在此通道。
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        const auto& visual=FindVisual(visuals,bullet.visual);
        if(bullet.beamLength>0) {
            beams_.push_back(&bullet);
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
        const float d=spriteDepth(visual,bullet.radius*visual.bulletDiameterScale,bullet.position,
                                  &bullet.animation.player.Sample(),bullet.velocity);
        // 同深度按插入顺序稳定绘制，光晕先于自身弹体，避免光效盖住前方角色或道具。
        if(visual.glow && !visual.archive && distance<=8.0F*8.0F && glowCounts_[bullet.visual]++<2048)
            queue({Kind::Glow,d,0,&bullet});
        queue({Kind::Bullet,d,0,&bullet});
    });

    if(!beams_.empty()) {
        std::sort(spriteDepths_.begin(),spriteDepths_.end());
        spriteDepths_.erase(std::unique(spriteDepths_.begin(),spriteDepths_.end()),spriteDepths_.end());
    }
    for(const Bullet* beam:beams_) {
        const float first=depth(beam->position),last=depth(beam->BeamEnd()),difference=last-first;
        beamCuts_={0,1};
        // 一条光束可能同时在演员前后：在精灵平面处切开，UV 仍沿整束连续，不能整束按中点排序。
        if(std::abs(difference)>1e-5F) {
            const auto begin=std::upper_bound(spriteDepths_.begin(),spriteDepths_.end(),std::min(first,last));
            const auto end=std::lower_bound(spriteDepths_.begin(),spriteDepths_.end(),std::max(first,last));
            for(auto it=begin;it!=end;++it) {
                const float t=(*it-first)/difference;
                if(t>1e-5F && t<1-1e-5F)beamCuts_.push_back(t);
            }
        }
        std::sort(beamCuts_.begin(),beamCuts_.end());
        for(std::size_t i=1;i<beamCuts_.size();++i) {
            const float from=beamCuts_[i-1],to=beamCuts_[i];
            transparentDraws_.push_back({Kind::Beam,first+(from+to)*0.5F*difference,0,beam,from,to});
        }
    }
    // 沿视线投影的深度才决定遮挡；到相机的欧氏距离会在斜视/大公告板重叠时反转顺序。
    std::stable_sort(transparentDraws_.begin(),transparentDraws_.end(),
                     [](const auto& left,const auto& right){return left.depth>right.depth;});
    rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ALPHA);
    for(const auto& command:transparentDraws_) {
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
            if(IsTextureValid(visual.texture))DrawSprite(camera,screenUp,Spec(visual,height,pickup.animation),pickup.position);
            else DrawSphere(pickup.position,height*0.35F,RED);
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
                DrawAnimatedBeam(camera,visual.texture,*visual.archive,frame,start,end,width*2);
            } else if(!beam.Dangerous() && beam.beamGrowSeconds<=0) {
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
                                       std::span<const Enemy*> enemies,
                                       const Boss* boss,
                                       Vector3 playerPosition) const {
    rlDisableDepthTest();
    rlDisableDepthMask();
    int shownBullets = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (shownBullets++ >= 256) return;
        if (bullet.beamLength > 0) {
            DrawCylinderWiresEx(bullet.position, bullet.BeamEnd(), bullet.radius, bullet.radius,
                                10, Color{255, 230, 90, 230});
            return;
        }
        DrawSphereWires(bullet.position, bullet.radius, 7, 10,
                        Color{255, 230, 90, 230});
    });
    for (const Enemy* enemy : enemies) {
        DrawSphereWires(enemy->position, enemy->hitRadius, 9, 12,
                        Color{65, 240, 250, 240});
        const Vector3 muzzle = enemy->MuzzlePosition(playerPosition);
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
