// 本文件声明五面咲夜及使魔的独立源时钟执行器，只供测试，不参与生产编排。
#pragma once
#include "stage5_reference_program.hpp"
#include "raylib.h"
#include <array>
#include <vector>
namespace koumakyou::stage5::reference {
/// @brief 单次源发射快照；位置为像素，速率为像素/帧，反弹次数按每颗弹独立保存。
struct CombatEmission {Vector3 position{},velocity{};int type=0,color=0,bounces=0;bool reflectLower=true;};
/// @brief 源95出生请求；sub在本关夹具内编号，位置为源像素，生命/分数为原始点数。
struct CombatSpawn {int sub,health,score,item;Vector3 position;};
/// @brief 源102/118请求，position/radius为像素、axis为无量纲源XYZ，color为独立整数ARGB。
struct ExternalEffect {int type,count;std::uint32_t color;Vector3 position;Vector3 axis{};float radius=0;};
/// @brief 固定60 Hz执行已审阅的道中、关底与使魔，独立维护移动、调用栈、周期发射和项目实例随机流。
/// 记录生命/时限/卡号、102/118与清弹请求，不执行生命回调、粒子运动、原生全局RNG和弹体积分；调用方另验完整遭遇。
/// 每步只保留出生快照，时停改向不含往帧候选；实际弹池的候选筛选及随机顺序由独立原生扩展用例检查。
class CombatReference {
public:
    /// @brief difficulty为E/N/H/L的0…3；sub支持13/18、24/36/43、46/49/52/55/60/63及27…34、35、38…41；rank为0…32。
    /// point为源像素；Boss独立入口继承移动动画表，使魔无边界/移动姿态。非法入口抛invalid_argument。
    CombatReference(unsigned difficulty,int sub,Vector3 point,unsigned seed,int rank);
    /// @brief 推进一内容帧并清空本帧发射/音效/效果/清弹；父时钟在子调用期间冻结，返回后同帧继续。
    void Tick(Vector3 player);
    /// @brief 切至使魔已审阅的Sub35死亡后继，保留当前运动、动画和随机流；仅供已出生使魔的击破对照。
    void DefeatFamiliar();
    /// @brief 推进项目xorshift流并返回[0,range)，range须非负有限；不代表原生全局随机序列。
    float Random(float range);
    Vector3 position{};bool active=true,damageable=true,interactable=true,timeStopped=false;int phaseFrames=0,redirects=0;
    int animation=128,health=0,threshold=0,timeout=0,card=-1,interrupt=0;float radius=0;
    std::vector<CombatEmission> emissions;std::vector<int> sounds,drops;std::vector<CombatSpawn> spawns;
    std::vector<ExternalEffect> effects;bool clearBullets=false;
private:
    struct Context {int sub=0,pc=0,frame=0,compare=0;std::array<float,12> variables{};};
    float Value(float id,Vector3 player) const;
    float& Variable(float id);
    void Execute(const Instruction& instruction,Vector3 player);
    void Interpolate(int frames,Vector3 target);
    void Emit(Vector3 player);
    void EmitClockArc(Vector3 player,int pulse);
    unsigned difficulty_,random_;int rank_;
    Context context_;std::vector<Context> stack_;
    Vector3 velocity_{},offset_{},start_{},delta_{},lower_{32,48,0},upper_{352,176,0};
    float direction_=0,speed_=0;int duration_=0,left_=0,period_=0,elapsed_=0,sound_=-1,bounces_=1,pose_=0;
    int nativeClock_=-1;
    bool interpolating_=false,clamped_=true,suppressed_=false,movingPoses_=true;Instruction shot_{};
};
}
