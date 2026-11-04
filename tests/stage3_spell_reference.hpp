// 本文件声明三面新增符卡的独立源执行对照；只运行测试夹具，不进入生产引擎。
#pragma once
#include "stage3_reference_program.hpp"
#include "raylib.h"
#include <array>
#include <vector>
namespace koumakyou::stage3::reference {
/// @brief 一发源弹的生成快照；位置为逻辑像素，速率为像素/帧，加速度为像素/帧²。
struct SpellEmission {
    Vector3 position{},velocity{},acceleration{};
    int type=0,color=0,flags=0;
    float turnAngle=0,turnSpeed=0,turnDelay=0;
};
/// @brief 单次源召唤请求；坐标为源像素，生命、掉落及分数保留原始值，不模拟子演员执行。
struct SpellSpawn {Vector3 position{};int health=0,item=0,score=0;};
/// @brief 源请求快照；position/半径是源像素，axis无量纲，color保存原始32位ARGB。只记录请求，不模拟粒子积分。
struct ExternalEffect {int type=0,count=0;std::uint32_t color=0;Vector3 position{},axis{};float radius=0;};
/// @brief 三面 Boss 及召唤演员所用语义的测试执行器；固定 60 Hz，未知指令抛异常。
/// 调用栈复制原上下文，持续发射函数和局部寄存器随调用继承、随返回恢复；星形几何另由实例独占。
/// 不模拟生命回调、原生全局随机数、辅助动画像素和弹体离场，不能充当全关完整原生基准。
class SpellReference {
public:
    /// @brief difficulty为0…3；入口4/7、19…48，另允许9的首130帧效果窗；越窗或未知指令拒绝。
    /// 坐标为源像素，seed为实例随机种子；19/20/48仅验证退场运动、音效、清弹和效果，生命回调另验。
    SpellReference(unsigned difficulty,int sub,Vector3 sourcePosition,unsigned seed);
    /// @brief 推进一帧，先应用上一帧位移，再执行源指令和持续函数；清空本帧发射/音效快照。
    void Tick(Vector3 sourcePlayer);
    /// @brief 项目实例 xorshift 的下一次浮点读取，用于检查读取次数及顺序，range 可负。
    float Random(float range);
    Vector3 position{};
    int phaseFrames=0;
    bool damageable=true;
    bool clearBullets=false;
    std::vector<SpellEmission> emissions;
    std::vector<SpellSpawn> spawns;
    std::vector<int> sounds;
    std::vector<ExternalEffect> effects;
    /// @brief 辅助槽保留源脚本、重启次数和启动后的60Hz整帧年龄；空槽script=-1，不解释ANM本身。
    struct Auxiliary {int script=-1,frames=0;unsigned revision=0;};
    std::array<Auxiliary,8> auxiliary{};
    /// @brief 累计实际执行过的子程序编号，仅用于测试覆盖统计，不代表静态可达闭包。
    std::array<bool,61> visited{};
private:
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};bool star=false;};
    float Value(float value,Vector3 player) const;
    float& Variable(float id);
    void Execute(const Instruction& instruction,Vector3 player);
    void Interpolate(int frames,Vector3 target,int power);
    void Emit(const Instruction& instruction,Vector3 player,Vector3 origin,float angle,float firstSpeed);
    void Star(Vector3 player);
    Context context_;
    std::vector<Context> stack_;
    unsigned difficulty_,random_;
    Vector3 velocity_{},interpolationStart_{},interpolationDelta_{},starStart_{},starPlayer_{},offset_{};
    std::array<float,6> angles_{},rank_{-0.5F,0.5F,0,0,0,0};
    float direction_=0,speed_=0,movementAcceleration_=0,acceleration_=0,accelerationAngle_=0;
    float turnAngle_=0,turnSpeed_=0,turnDelay_=0;
    int interpolationDuration_=0,interpolationLeft_=0,power_=1;
    bool interpolating_=false,suppressed_=false,clamped_=true;
    Instruction shot_{};
    Vector2 lower_{32,48},upper_{352,144};
    int ticks_=0,entry_=0;
};
}
