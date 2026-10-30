// 本文件声明六面道中咲夜与已接入蕾米莉亚战斗的独立源时钟执行器，只供测试，不参与生产编排。
#pragma once
#include "stage6_reference_program.hpp"
#include "reference_ambient_effects.hpp"
#include "stage6_effect_reference.hpp"
#include "raylib.h"
#include <array>
#include <vector>
namespace koumakyou::stage6::reference {
/// @brief 单次源发射快照；空间量为源像素，时间为内容帧；turn为弧度，angularVelocity为弧度/帧，speedAcceleration为像素/帧²。
struct CombatEmission {Vector3 position{},velocity{};int type=0,color=0;float delay=0,turn=0;int turns=0;Vector3 acceleration{};float accelerationFrames=-1;
    float angularVelocity=0,angularFrames=0,speedAcceleration=0;int bounces=0;bool reflectLowerZ=true;int flags=0;};
/// @brief 本关95出生请求；位置为源像素，编号只在独立夹具内使用，生命/分数为原始点数。
struct CombatSpawn {int sub,health,score,item;Vector3 position;};
/// @brief 原生星阵光束出生快照；位置和长度为源像素，direction 为单位方向，grow 为60 Hz内容帧。
struct CombatBeam {Vector3 position,direction;float length;int grow;};
/// @brief 固定60 Hz执行已审阅的六面战斗源指令，独立维护移动、子调用和项目随机流。
/// 记录生命/时限/卡号及原生6/10请求，不执行生命回调、粒子积分、原生全局RNG和弹体积分；调用方另验完整遭遇。
class CombatReference {
public:
    /// @brief difficulty为E/N/H/L的0…3；支持Sub9/14、N/H/L的Sub19/21/25/28/31/67及五组N与H/L关底符卡入口。
    /// rank为10…32，shot为四机体的0…3。
    /// point为源像素；独立入口继承对应角色入场的移动姿态和边界。chargingPulses 为 Sub21 继承的 0…16 剩余脉冲。
    /// 非 Sub21 入口须传零；非法参数抛 invalid_argument。
    CombatReference(unsigned difficulty,int sub,Vector3 point,unsigned seed,int rank,int shot=0,int chargingPulses=0);
    /// @brief 推进一内容帧并清空本帧发射/音效；父时钟在子调用期间冻结，返回后同帧继续。
    void Tick(Vector3 player);
    /// @brief 推进项目xorshift流并返回 [0,1) 比例乘以有限 range；range 可负，不代表原生全局随机序列。
    float Random(float range);
    Vector3 position{};bool active=true,damageable=true,interactable=true;int phaseFrames=0;
    int animation=128,health=0,threshold=0,timeout=0,card=-1;float radius=0;
    std::vector<CombatEmission> emissions;std::vector<int> sounds,drops;
    std::vector<CombatSpawn> spawns;
    std::vector<CombatBeam> beams;
    std::vector<ExternalEffect> effects;
    std::vector<koumakyou::reference::AmbientEffect> ambientEffects;
    // 原生地雷扩展的独立环境输入与本帧输出，均为源单位快照；Tick复制environment后执行发射/修改。
    // 调用方提供可控场景而非生产池快照，本执行器不积分弹体，也不据此证明完整原生回收等价。
    std::vector<CombatEmission> environment,projectiles;
private:
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};};
    float Value(float id,Vector3 player) const;
    float& Variable(float id);
    void Execute(const Instruction& instruction,Vector3 player);
    void Interpolate(int frames,Vector3 target);
    void Emit(Vector3 player,const Vector3* point=nullptr);
    void StarLattice(bool projectiles,Vector3 player);
    void SeedMines();
    void ReleaseMines(bool randomDirections);
    unsigned difficulty_,random_;int rank_,shotType_;
    koumakyou::reference::AmbientReference ambient_;
    int nativeCallback_=-1; // 隔离战斗继承Sub18安装的6；Sub31新生使魔不继承，最终符由源122改为10。
    Context context_;std::vector<Context> stack_;
    Vector3 velocity_{},offset_{},start_{},delta_{},lower_{32,48,0},upper_{352,128,0};
    float direction_=0,speed_=0,angular_=0;int duration_=0,left_=0,period_=0,elapsed_=0,sound_=-1,pose_=0;
    float delay_=0,turn_=0;int turnCount_=0;
    Vector3 acceleration_{}; // 82 的定向加速度，源像素/内容帧²；不随父演员之后的移动旋转。
    bool radialAcceleration_=false;float accelerationFrames_=-1;
    float projectileAngular_=0,polarAcceleration_=0; // 82在Velocity模式下将第4/5参数解释为速率增量和角速度。
    std::array<int,5> poses_{128,131,132,129,130};
    std::array<float,6> rankInfluence_{-0.5F,0.5F,0,0,0,0}; // 源速度单位，清弹恢复默认值，131 可独立覆盖。
    bool interpolating_=false,clamped_=true,suppressed_=false,movingPoses_=true;Instruction shot_{};
};
}
