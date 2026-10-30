// 本文件声明四面道中、非符及全部元素符卡的独立源语义执行器，像素和内容帧仅用于测试。
#pragma once
#include "stage4_reference_program.hpp"
#include "raylib.h"
#include <array>
#include <vector>
namespace koumakyou::stage4::reference {
/// @brief 源发射快照，位置为像素、速率为像素/帧，加速度为像素/帧²；不积分普通弹。
struct CombatEmission {
    Vector3 position{},velocity{};int type=0,color=0;float acceleration=0,angularVelocity=0,angularFrames=0;
    Vector3 accelerationVector{};float accelerationFrames=-1;float speedAcceleration=0;
    // 转向参数按每次发射复制，delay 为内容帧、speed 为像素/帧、angle 为弧度。
    int flags=0,turnCount=1;float turnDelay=0,turnAngle=0,turnSpeed=0;
};
/// @brief 源光束引用及生成参数，angle 为源平面弧度，尺寸为像素，时间为内容帧。
/// 引用失效独立于脚本调用栈；清引用不清已发光束。
struct CombatBeam {
    bool active=false;int slot=0,color=0,age=0,grow=0,sustain=0,fade=0,warning=0,tail=0;
    float angle=0,start=0,end=0,width=0;Vector3 anchor{};
    float speed=0,lengthLimit=0; // 伸长光束的像素/帧速率与最大长度；零速率表示固定线段。
};
/// @brief 一次源召唤请求，不递归执行子演员；生命、分数和道具保持原始点数/编号。
struct CombatSpawn {int sub=0,health=0,item=0,score=0;Vector3 position{};};
/// @brief 源102/118请求快照；position/radius为像素，axis为无量纲源XYZ，color保留整数ARGB。
struct ExternalEffect {int type,count;std::uint32_t color;Vector3 position;Vector3 axis{};float radius=0;};
/// @brief 固定 60 Hz 执行四面 Sub21、27/37、39…41、96 和全部命名元素/组合入口，独立维护源坐标、调用栈、周期发射和光束。
/// 记录生命设置、回调目标、102/118及清弹请求，但不执行生命回调、粒子运动、原生全局 RNG 或弹体离场。
/// 光束 active 表示名义寿命内的可对照窗口，不证明原生在窗口边界帧的回收条件。
class CombatReference {
public:
    /// @brief difficulty 和 shot 为 0…3，rank 为 0…32；seed 为项目实例 xorshift 种子。
    CombatReference(unsigned difficulty,unsigned shot,int sub,Vector3 position,unsigned seed,int rank=16);
    /// @brief 推进一内容帧，清空本帧发射/召唤/音效/效果及清弹请求；调用期间父上下文冻结，返回后同帧继续。
    void Tick(Vector3 sourcePlayer);
    /// @brief 推进本实例的项目 xorshift 流，返回 [0,range)；range 须为非负有限数，不代表原生全局 RNG。
    float Random(float range);
    Vector3 position{};
    bool damageable=true,interactable=true;
    // 原生命寄存器与阈值保持原始 HP；card 为源 0 起算编号，-1 表示未宣告。
    int health=0,threshold=0,timeout=0,card=-1,healthCallback=-1,deathCallback=-1,timeoutCallback=-1;
    std::vector<CombatEmission> emissions;
    std::vector<CombatSpawn> spawns;
    std::vector<int> sounds;
    std::vector<ExternalEffect> effects;
    bool clearBullets=false;
    std::vector<CombatBeam> beams;
    std::array<bool,119> visited{};
private:
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};};
    void Execute(const Instruction& instruction,Vector3 player);
    float Value(float id,Vector3 player) const;
    float& Variable(float id);
    void Interpolate(int frames,Vector3 target,int power);
    void Emit(Vector3 origin,Vector3 player);
    void SelectCombinations();
    unsigned difficulty_,shot_,random_;int rank_;
    Context context_;std::vector<Context> stack_;
    Vector3 velocity_{},offset_{},start_{},delta_{},boundsMin_{},boundsMax_{};
    float direction_=0,speed_=0,acceleration_=0,angularVelocity_=0,angularFrames_=0;
    int duration_=0,left_=0,power_=2,period_=0,elapsed_=0,sound_=-1,slot_=0;
    int turnCount_=1;float turnAngle_=0,turnSpeed_=0;
    bool interpolating_=false,clamped_=false,suppressed_=false;
    Instruction shotInstruction_{};
    std::array<int,32> handles_{};
};
}
