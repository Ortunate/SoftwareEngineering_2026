// 本文件实现第 1 面普通演员的命名行为与资源绑定；Boss 专属行为由角色模块拥有。
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::stage1 {
namespace {
// 原作在出生帧配置自动瞄准弹并设置随机初相；计数器与移动时钟独立。
behavior::Routine LunarTurningFairy(behavior::Actor& a,int variant) {
    a.ConfigureEmitter({skill::Formation::AimedFan,0,6,1,1,8.03571415F,0,0,0,skill::SpawnMedium});
    // 原周期在设置时按当前 rank 缩放；之后 rank 变化不会重写已保存的计数周期。
    const int period=120+24-(48*a.skills.rank)/32;
    a.SetEmitterPeriod(period,static_cast<int>(a.RandomIndex(period)));
    if(variant==2) {
        co_await a.At(100);a.motion.Apply({.angularVelocity=1.17809725F});
        co_await a.At(200);a.motion.Apply({.angularVelocity=0.0F});
    } else {
        co_await a.At(40);a.motion.Apply({.angularVelocity=-1.47262156F});
        co_await a.At(120);a.motion.Apply({.angularVelocity=variant==1?1.17809725F/2:1.17809725F});
        co_await a.At(220);a.motion.Apply({.angularVelocity=0.0F});
    }
    co_await a.At(10000);a.finished=true;
}
behavior::Routine LunarTurning(behavior::Actor& a) {return LunarTurningFairy(a,0);}
behavior::Routine LunarGentleTurning(behavior::Actor& a) {return LunarTurningFairy(a,1);}
behavior::Routine LunarLateTurning(behavior::Actor& a) {return LunarTurningFairy(a,2);}
void ConfigureLunarTurning(behavior::Pattern& pattern,native_combat::Difficulty difficulty,behavior::Entry routine) {
    if(difficulty==native_combat::Difficulty::Lunatic) {pattern.routine=routine;pattern.movement.count=0;}
}
}
void ConfigureTurningFairy(behavior::Pattern& p,native_combat::Difficulty d) {ConfigureLunarTurning(p,d,LunarTurning);}
void ConfigureGentleTurningFairy(behavior::Pattern& p,native_combat::Difficulty d) {ConfigureLunarTurning(p,d,LunarGentleTurning);}
void ConfigureLateTurningFairy(behavior::Pattern& p,native_combat::Difficulty d) {ConfigureLunarTurning(p,d,LunarLateTurning);}
void ConfigurePassingDarkFairy(behavior::Pattern& p,native_combat::Difficulty d) {
    if(d==native_combat::Difficulty::Lunatic)
        p.skill=skill::TimedShot{70,{skill::Formation::AimedFan,1,2,5,1,3.75F,0,0,0.2617994F,
                                     skill::LaunchBoost|skill::SpawnSmall,15,1.1F}};
}
void ConfigureShootingDarkFairy(behavior::Pattern& p,native_combat::Difficulty difficulty) {
    auto& shot=p.skill;
    switch(difficulty) {
        case native_combat::Difficulty::Easy:
            shot->shot.count=3;shot->shot.spacing=0.7853982F;shot->shot.spatialCount=9;break;
        case native_combat::Difficulty::Normal: break;
        case native_combat::Difficulty::Hard:
            shot->shot.count=9;shot->shot.layers=2;shot->shot.lastSpeed=1.33928573F;
            shot->shot.spacing=0.3926991F;shot->shot.spatialCount=27;break;
        case native_combat::Difficulty::Lunatic:
            shot->shot.count=11;shot->shot.layers=2;shot->shot.lastSpeed=1.33928573F;
            shot->shot.spacing=0.157079637F;shot->shot.spatialCount=33;break;
    }
}
behavior::Spawn ConfigureSpawn(const behavior::Spawn& spawn,native_combat::Difficulty difficulty) {
    auto result=spawn;result.difficulty=difficulty;
    if(result.pattern.configureDifficulty)result.pattern.configureDifficulty(result.pattern,difficulty);
    return result;
}
void ConfigureBehavior(behavior::Actor& a,bool spatial) {
    a.skills.bulletBase=kNativeBulletBase;a.skills.laserBase=kNativeLaserBase;a.skills.fallback=kBlueBullet;
    a.skills.spatial=spatial;
    if(!spatial)a.liftedMotion.reset();
}
} // namespace koumakyou::stage1
