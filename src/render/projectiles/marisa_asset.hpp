// 本文件声明魔理沙资产的实例参数和动画采样；不管理模型、纹理或游戏伤害资源。
#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace koumakyou::projectiles {
inline constexpr int MarisaTierCount[]{1,4,3,4};
// kind 为主弹/导弹/激光/爆破 0..3，tier 为本类零起始档位。
// position/length/radius 为世界单位，direction 必须为单位向量，scale 为正的统一缩放。
// age、fadeAt 为秒；color 为 RGB 乘数 [0,1]，opacity 为 [0,1]，strength 为光效强度 [0,2]。
// trailLengthScale/trailWidthScale 是正的拖尾长宽倍数，默认 1，只作用于主弹与导弹光效。
// 激光从 position 朝 direction 延伸；其他模型原点位于中心。调用方持有并共享 glTF/Shader 资源。
struct MarisaInstance {int kind=0,tier=0;Vector3 position{},direction{0,1,0},color{1,1,1};float scale=1,length=3,radius=.1F,age=.1F,fadeAt=2,strength=.8F,opacity=1,trailLengthScale=1,trailWidthScale=1;};
// opacity 为方便审阅的峰值归一化透明度，nativeOpacity 为原 ANM 透明度；游戏额外 Alpha 51 由调用方相乘。
// bodyScale 表达静态档位及内球判定比例，growth 仅供外部爆破光效使用，不扩大内球碰撞。
struct MarisaVisualState {float opacity=1,nativeOpacity=1,growth=1,bodyScale=1,haloScale=1;bool alive=true;};
/// @brief 采样当前档位和时间；非法参数抛 invalid_argument，无资源所有权或副作用。
inline MarisaVisualState SampleMarisa(const MarisaInstance& p){
 if(p.kind<0||p.kind>3||p.tier<0||p.tier>=MarisaTierCount[p.kind])throw std::invalid_argument("Marisa kind/tier");
 for(float v:{p.position.x,p.position.y,p.position.z,p.direction.x,p.direction.y,p.direction.z,p.color.x,p.color.y,p.color.z,p.scale,p.length,p.radius,p.age,p.fadeAt,p.strength,p.opacity,p.trailLengthScale,p.trailWidthScale})if(!std::isfinite(v))throw std::invalid_argument("Marisa non-finite parameter");
 float norm=p.direction.x*p.direction.x+p.direction.y*p.direction.y+p.direction.z*p.direction.z;
 if(std::abs(norm-1)>1e-4F||p.scale<=0||p.trailLengthScale<=0||p.trailWidthScale<=0||p.length<=0||p.radius<=0||p.age<0||p.fadeAt<0||p.strength<0||p.strength>2||p.opacity<0||p.opacity>1||p.color.x<0||p.color.x>1||p.color.y<0||p.color.y>1||p.color.z<0||p.color.z>1)throw std::invalid_argument("Marisa parameter range");
 MarisaVisualState s;float frame=p.age*60;int t=p.tier;
 if(p.kind==0)s.nativeOpacity=192/255.F;
 if(p.kind==1){constexpr float scale[]{.65F,.75F,.9F,1};s.bodyScale=scale[t];s.nativeOpacity=128/255.F;}
 if(p.kind==2){constexpr float width[]{.5F,.75F,1};s.haloScale=width[t]/.5F;s.opacity=std::min(frame/20,1.F)*std::clamp(1-(p.age-p.fadeAt)/.5F,0.F,1.F);s.nativeOpacity=s.opacity*(t==0?128:160)/255.F;s.alive=p.age<p.fadeAt+.5F;}
 if(p.kind==3){constexpr float diameter[]{32,42,48,48};s.bodyScale=diameter[t]/32;s.opacity=frame<6?1:std::clamp((20-frame)/14,0.F,1.F);s.nativeOpacity=s.opacity*96/255.F;s.growth=(.8F+(.075F+.025F*t)*std::min(frame,20.F))/.8F;s.alive=frame<20;}
 s.bodyScale*=p.scale;s.opacity*=p.opacity;s.nativeOpacity*=p.opacity;return s;
}
}
