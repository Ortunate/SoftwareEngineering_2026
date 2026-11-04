// 本文件定义独立于弹幕/演员池的世界坐标演出粒子及512槽生命周期；不参与碰撞、计分或攻击随机流。
#pragma once
#include "resources/visual_assets.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace koumakyou::effects {
/// @brief 已核验并接入的非拥有型效果；编号属于项目语义，不用于选择ECL子程序。
enum class Kind { Burst, Flash, Charge, SlowCharge, Ambient, Attached, Drift, Graze };
/// @brief 环境效果的安装策略：Active不检查生命，Living仅在HP>0执行；隐藏时两者都冻结。
enum class AmbientMode { Disabled, Active, Living };
/// @brief 演员独占的附着中心与共享目标半径，均为世界单位；效果仅保存弱引用，不延长演员寿命。
/// 同一演员后续附着请求改变半径时，已有轨道也读取新目标；销毁此状态使全部轨道冻结并淡出。
class AttachmentAnchor {
public:
    /// @brief 原子更新本固定步末的位置与非负目标半径；非法输入抛invalid_argument，原状态不变。
    void Follow(Vector3 position,float radius);
    [[nodiscard]] Vector3 Position() const {return position_;}
    [[nodiscard]] float Radius() const {return radius_;}
private:
    Vector3 position_{};
    float radius_=0;
};
/// @brief 一次按值复制的演出请求；position为世界坐标，count为正数，color为实际RGBA。
/// contentFrame仅记录演员当前阶段的60Hz内容时刻，池不据此推算事件时序；请求不持有演员或资源。
/// Ambient携带世界单位/秒的velocity和世界单位/秒²的acceleration；Attached仅接受count=1，
/// axis为无量纲世界方向，attachment弱引用跟随中心；零轴允许且不产生位移。其他种类忽略这些字段。
struct Request {
    Kind kind=Kind::Charge;Vector3 position{};int count=1;Color color=WHITE;double contentFrame=0;
    Vector3 velocity{},acceleration{},axis{};
    std::weak_ptr<const AttachmentAnchor> attachment{};
};
/// @brief 成对环境粒子的独立实例状态；时钟以60Hz内容帧计，相位为弧度，空间输入输出全部是世界单位。
/// 调用者每固定步调用Step；隐藏/停用时不调用，恢复不重置时钟。局部视觉随机流不借用攻击随机源。
class AmbientEmitter {
public:
    /// @brief 仅在演员新生/复用时重置；seed=0按1处理，技能交接保留同一实例的时钟与相位。
    void Reset(std::uint32_t seed=1);
    /// @brief 推进0.5或1内容帧；在到期步返回两个按值请求，否则返回空。position为本步末世界位置。
    /// phaseFrame是调用者阶段时刻的元数据；非法输入抛invalid_argument且不推进状态。
    [[nodiscard]] std::vector<Request> Step(double contentFrames,Vector3 position,double phaseFrame);
private:
    std::uint32_t NextRandom();
    std::uint32_t random_=1;
    double clock_=0;
    std::uint64_t previous_=0; // 出生复制清零模板后与current相等，首次回调仅推进，不在0帧发射。
    float phase_=0;
};
/// @brief 返回etama4图集内的动画脚本；未知Kind抛invalid_argument，不回退其他素材。
[[nodiscard]] int AnimationScript(Kind kind);
/// @brief 每槽独占动画与运动状态，借用Bind提供的ANM；空间量均为世界单位，速度/加速度为每秒及每秒平方。
/// frame是本效果起算的60Hz内容时钟；收束粒子保存出生点，不能改成速度积分；Drift与Graze分别保存慢速漂散和快速擦弹的固定加速度；均由各自ANM结束寿命。
struct Particle {
    bool active=false,initialized=false;
    Kind kind=Kind::Charge;
    Vector3 position{},origin{},velocity{},acceleration{},direction{};
    VisualId visual=kInvalidVisualId;
    double frame=0;
    anm::Player animation;
    std::weak_ptr<const AttachmentAnchor> attachment{};
    Vector3 axis{};
    // 源工厂保留槽内半径和相位；普通失活/复用不能清零，仅整池Clear清零。相位为弧度。
    float orbitRadius=0,orbitPhase=0;
    double fadeFrames=0; // 释放后冻结轨道，按60Hz内容帧淡出16帧；120Hz允许半帧。
    bool released=false;
};
/// @brief 512个独立演出槽，循环扫描复用且满池只丢弃视觉；不占用敌弹容量。
/// 默认120Hz，60Hz仅供原作差分；Clear必须先于关卡卸载，并释放所有ANM借用。
class ExternalEffectSystem {
public:
    explicit ExternalEffectSystem(int updateHz=120);
    /// @brief 借用etama4档案和当前关卡外观槽；有活动粒子时拒绝重绑，缺少必需脚本抛invalid_argument。
    void Bind(const anm::Archive& archive,VisualId visual);
    /// @brief 同帧创建count粒子，启动ANM后覆盖请求颜色；返回成功数，未Bind或非法请求抛异常且不改池。
    /// 池满不覆盖旧槽，也不影响演员/奖励；局部效果随机源不消费任何攻击随机数。
    std::size_t Spawn(const Request& request);
    /// @brief 累计非负有限秒数补算；每步先运动再ANM，暂停时调用方停止推进。
    /// 演员请求在本步Update前交付；碰撞擦弹在本步Update后创建，首个运动步留到下次更新。附着中心须逐固定步更新。
    void Update(float seconds);
    /// @brief 释放全部槽和档案借用，重置循环游标/统计/局部随机源；seed=0按1处理。
    void Clear(std::uint32_t seed=1);
    [[nodiscard]] std::size_t ActiveCount() const;
    [[nodiscard]] std::size_t DroppedSpawns() const {return dropped_;}
    [[nodiscard]] const anm::Archive* Source() const {return archive_;}
    /// @brief 只读借用活动槽；回调期间不得推进、生成、清池或卸载素材。
    template<class Function> void ForEachActive(Function&& function) const {
        for(const auto& particle:particles_)if(particle.active)function(particle);
    }
private:
    void Tick();
    std::uint32_t NextRandom();
    float Random();
    std::array<Particle,512> particles_{};
    const anm::Archive* archive_=nullptr;
    VisualId visual_=kInvalidVisualId;
    std::size_t cursor_=0,dropped_=0;
    std::uint32_t random_=1;
    int updateHz_;
    double ticks_=0;
};
} // namespace koumakyou::effects
