// 本文件定义带参行为编排器；生产以 120 Hz 更新，内容帧号保持 60 Hz，测试可选 60 Hz 对照原作。
#pragma once
#include "gameplay/actors/skill.hpp"
#include "gameplay/effects/actor_effects.hpp"
#include <coroutine>
#include <exception>
#include <vector>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <stdexcept>
namespace koumakyou::behavior {
/// @brief 行为积分频率；生产默认 Hz120，Hz60 供独立原作差分，均不改变内容帧号的 60 Hz 单位。
enum class UpdateRate { Hz60=60, Hz120=120 };
class Actor;
class Routine;
/// @brief 无捕获的命名行为入口；返回只借用当前 Actor 的独占协程，不包含原始脚本编号。
using Entry=Routine(*)(Actor&);
/// @brief 待交接回调的实际原因；阈值钳制 HP 后仍可区分伤害完成与超时，不编码关卡或脚本编号。
enum class CallbackCause { None, HealthThreshold, Defeat, Timeout };
/// @brief 可共享的行为配方；时间为出生起算的 60 Hz 帧，配置按值复制，执行状态由 Actor 独占。
struct Pattern {
    movement::Path movement;
    std::optional<skill::TimedShot> skill;
    int initialAnimation=0, stoppedAnimation=-1, stoppedAnimationFrame=60, deathEffect=1;
    Vector3 muzzleOffset{}; // 世界 XYZ 偏移，与运动位置直接相加。
    int endFrame=10000; // 逻辑寿命终点，必须覆盖路径、技能与动画的全部关键帧。
    bool spatialShots=false; // 启用真实三维瞄准和等面积方向采样；默认沿用平面高度坡度。
    // 复杂路线与攻击可接管初始化后的演员；此时 movement 仅提供初速，不得再配置 keys/skill/停止动画。
    Entry routine=nullptr;
    Entry onDefeated=nullptr; // 普通演员击破后的命名行为；奖励只结算一次，回调期间本体不参与接触。
    bool rotateAnimation=false; // 将路径的 X/Z 方向写入公告板局部 Z 旋转；不改变移动或碰撞几何。
};
/// @brief 时间轴直接提交的行为演员配置；position/半径为世界单位，生命与分数为原始点数。
/// sourceItem=-2 不掉落、-1 全关轮换、0/1/2 小P/分数/大P；randomX 按 seed 在 randomXRange 世界区间取值一次。
/// visual 及素材基址属于当前关卡；retireOutside 须先进入 retirementBounds 及可选 retirementHeights 才允许越界回收。
struct Spawn {
    Vector3 position{};
    Pattern pattern;
    bool mirrored=false,randomX=false;
    float sourceHealth=32;
    int sourceScore=300;
    unsigned seed=1;
    int sourceItem=-1;
    VisualId visual=kInvalidVisualId;
    float hitRadius=0.55F;
    Vector2 randomXRange{-8.57142857F,8.57142857F};
    movement::Bounds retirementBounds;
    VisualId bulletBase=kInvalidVisualId,laserBase=kInvalidVisualId,defeatBase=kInvalidVisualId,fallback=kInvalidVisualId;
    bool retireOutside=true;
    bool damageable=true; // 初始受伤开关；阶段行为仍可在运行中修改。
    std::optional<Vector2> retirementHeights; // 可选世界 Y 回收闭区间，与 X/Z 同时入场后才允许越界回收。
    int initialAnimation=-1; // 可选的实例外观脚本，非负时覆盖配方初始动画；同一路线/技能可独立选色，不产生行为分支。
};
class Actor;
/// @brief 一段独占的行为协程；初始挂起，交给 Actor 后由 Actor 持有并销毁，不能复制。
/// 子行为的 60 Hz 内容时钟从零开始，调用期间父时钟冻结，返回后在同一更新步继续执行父行为。
class Routine {
public:
    struct promise_type {
        Actor* owner=nullptr;
        double frame=0; // 60 Hz 局部内容帧；120 Hz 每次更新前进 0.5 帧。
        int due=0;
        std::exception_ptr failure;
        Routine get_return_object() { return Routine{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { failure=std::current_exception(); }
    };
    using Handle=std::coroutine_handle<promise_type>;
    explicit Routine(Handle h):handle_(h) {}
    Routine(Routine&& other) noexcept:handle_(other.Release()) {}
    Routine(const Routine&)=delete;
    ~Routine() { if(handle_)handle_.destroy(); }
    struct Awaiter {
        Handle child;
        bool await_ready() const noexcept { return false; }
        void await_suspend(Handle parent);
        void await_resume() const noexcept {}
    };
    Awaiter operator co_await() && { return {Release()}; }
private:
    friend class Actor;
    Handle Release() { auto h=handle_;handle_=nullptr;return h; }
    Handle handle_;
};
/// @brief 等待当前行为达到指定 60 Hz 局部内容帧；已到期时同一更新步继续，不累加显示帧误差。
struct AtFrame {
    Actor& owner;
    int frame;
    bool await_ready() const;
    void await_suspend(Routine::Handle handle) const { handle.promise().due=frame; }
    void await_resume() const noexcept {}
};
/// @brief 一名敌人/Boss 的行为状态，不依赖具体关卡或 ECL 指令；位置、HP 与发弹属性跨阶段保留。
/// 每个演员至多持有 32 层独占行为；Reset/Start/Transition 和销毁须发生在 Update 之外，不支持重入。
class Actor {
public:
    /// @brief 固定该实例的更新频率；仅接受 Hz60/Hz120，其他枚举值抛 invalid_argument。
    /// Reset/Activate 保留频率，避免池复用或阶段交接时悄然改变积分步长。
    explicit Actor(UpdateRate rate=UpdateRate::Hz120);
    Actor(const Actor&)=delete;
    Actor& operator=(const Actor&)=delete;
    ~Actor();
    /// @brief 重置全部状态，position 为世界坐标；seed=0 按 1 处理，镜像只反转最终 X 位移。
    void Reset(Vector3 position,float hp,std::uint32_t seed,bool mirrored=false);
    /// @brief 复制通过 Valid 的出生配置，初始化独立运动、战斗和视觉状态，启动 Follow 及可选命名行为。
    /// 非法配置抛 invalid_argument；不持有配置引用，不加载素材。
    void Activate(const Spawn& spawn);
    /// @brief 接管非空根行为并销毁先前行为；传入协程须引用当前 Actor。
    /// 调用前应 Reset，或由 Transition 保留阶段状态；失败抛 invalid_argument/bad_alloc。
    void Start(Routine routine);
    /// @brief 累计非负有限秒数，以实例固定频率补算；回调触发后停止，剩余更新步保留到 Transition。
    void Update(float seconds,Vector3 player,BulletSystem& bullets);
    /// @brief 进入 pendingCallback，清空调用链、阈值和阶段时钟，保留运动与技能上下文。
    void Transition();
    /// @brief 普通演员的击破交接；首次调用返回 true，立即关闭伤害/接触并进入击破行为或回收。
    /// 调用方仅在 true 时结算分数、掉落和效果；须在 Update 之外调用，不能用于 Boss 阶段切换。
    bool Defeat();
    /// @brief 设置 HP≤0 的后继阶段，nullptr 关闭该回调；回调实际启动由调用方 Transition 决定。
    void SetDeathCallback(Entry entry) { deathCallback=entry; }
    /// @brief 设置 10…32 范围的自适应难度；越界值钳制，影响后续发射。
    void SetRank(int rank) { skills.rank=std::clamp(rank,10,32); }
    /// @brief 当前三维世界位置，直接返回运动状态，不执行坐标转换。
    [[nodiscard]] Vector3 Position() const { return motion.position; }
    /// @brief 当前最深层行为的 60 Hz 局部内容帧，120 Hz 时可含半帧；仅在已启动且尚未返回的行为中查询。
    [[nodiscard]] double Frame() const { return stack_.back().promise().frame; }
    /// @brief 构造非负局部帧的等待点，仅供当前行为 co_await；不会推进阶段总时钟。
    [[nodiscard]] AtFrame At(int frame) { return {*this,frame}; }
    /// @brief 循环回到局部 frame，在本帧继续执行；不能改成等待剩余时长，否则循环周期会偏移。
    void Rewind(int frame) {
        // 旧等待点已消费；回拨后若立即调用子行为，返回时不能再被上一轮的 due 挡住。
        auto& current=stack_.back().promise();current.frame=frame;current.due=frame;
    }
    /// @brief 为当前最深子行为增加非负局部内容帧；保留 120 Hz 的半帧余量，不推进阶段总时钟。
    /// 用于内容定义的脚本加速，后续已到期等待可在同一更新步继续。
    void ElapseLocalFrames(int frames) {
        if(frames<0)throw std::invalid_argument("Local frame advance must be nonnegative");
        stack_.back().promise().frame+=frames;
    }
    /// @brief 可重放的演员独立随机源，返回 [0,1) 比例乘以有限 range（可负）；调用顺序是行为一致性的一部分。
    float Random(float range);
    /// @brief 从同一实例随机流读取一次无符号整数并取模，返回 [0, bound)；bound 必须为正。
    /// 保留整数周期/离散分支的读取语义，不能用浮点比例乘 bound 代替。
    std::uint32_t RandomIndex(std::uint32_t bound);
    /// @brief 在边界附近反射随机方向，避免 Boss 向场外移动；角度范围为弧度。
    void RandomDirection(float minimum,float maximum,bool reflect=true);
    /// @brief 重新启动当前演员图集内的动画，包含相同编号的重启，独立动画年龄归零。
    void Animate(int script);
    /// @brief 在当前动画年龄提交本体中断，保留动画后续淡出/收势；同一固定步多次提交取最后一次。
    /// 必须已有活动动画；命令按 revision/id 排除重启后的旧中断，只保存值，不借用资源。
    void InterruptAnimation(int label);
    /// @brief 更换待机/收势/移动五个图集内脚本，并使本步末重新选择姿态；负值槽表示关闭。
    /// 阶段更换姿态组时必须重选，不能因移动方向未变而保留上一阶段的符卡动作。
    void SetPoses(std::array<int,5> scripts) { poses=scripts;pose_=-1; }
    /// @brief 以下释放只借用本次 Update 的玩家位置与弹池；只能在行为执行期间调用。
    void Fire(const skill::Shot& shot) { FireAt(shot,motion.position); }
    /// @brief 从指定世界基点加技能 offset 发射，共享演员随机流；只能在 Update 的行为执行期间调用。
    /// configuration 非空时只借用其发射快照，供周期发射保留配置时的 rank/弹道；不改变当前技能状态。
    void FireAt(const skill::Shot& shot,Vector3 position,const skill::State* configuration=nullptr);
    void Fire(const skill::Laser& laser) { skills.Fire(laser,motion.position,player_,*bullets_); }
    /// @brief 从指定世界基点释放光束；可选技能快照只在调用期间借用，与普通弹共用 offset 契约。
    void FireAt(const skill::Laser& laser,Vector3 position,const skill::State* configuration=nullptr) {
        (configuration ? *configuration : skills).Fire(laser,position,player_,*bullets_);
    }
    /// @brief 生成并记住 0～15 槽位的光束句柄；只能在 Update 中调用，池满清空该槽引用，不删除旧光束。
    void FireBeam(int slot,const skill::Laser& laser);
    /// @brief 即时借用当前槽仍活动的光束；仅在 Update 内有效，禁止保留指针，失活或复用返回 nullptr。
    [[nodiscard]] Bullet* Beam(int slot);
    /// @brief 只清除 16 个槽位引用，已发射光束按自身寿命继续；Reset 同样清除引用。
    void ClearBeamSlots() { beams_.fill({}); }
    /// @brief 复制当前技能/rank 和合法弹幕配方，更新持续发射配置；不立刻发弹，不重置既有周期计数。
    void ConfigureEmitter(const skill::Shot& shot);
    /// @brief 持续发射周期与初始计数均为 60 Hz 内容帧；period=0 停止，正周期要求已配置配方。
    /// 初始计数须在 [0,period)；配置保留跨子行为/阶段的执行状态，Reset 才无条件清除。
    void SetEmitterPeriod(int period,int initial=0);
    /// @brief 从最终世界发射点生成已配置图案，不再添加技能 offset；共享演员随机流，PlaySound 位决定是否播放已配置音效。
    /// 只能在 Update 内调用；持续发射在每个固定步的行为之后读取同一配置。
    void FireEmitterAt(Vector3 worldMuzzle);
    /// @brief 选择发射音效并设置当前持续配方的 PlaySound 位；负值只关播放位。
    /// 后续 ConfigureEmitter/显式发射按其 flags 重新决定是否播放，不能将旧音效开关强加给新配方。
    void SetEmitterSound(int sound);
    /// @brief 清除敌弹并恢复默认 rank 修正，仅在行为执行期间调用；不清除运动和 projectile 配置。
    void ClearBullets();
    /// @brief 访问当前玩家世界位置，只在行为执行期间有意义，不保存玩家对象引用。
    [[nodiscard]] Vector3 PlayerPosition() const { return player_; }
    /// @brief 以槽位顺序修改本次 Update 借用弹池中的敌方普通弹；不访问自机弹或光束。
    /// visitor 不得生成/清池或保留弹引用；在 Update 外调用抛 logic_error。
    void ModifyEnemyProjectiles(const std::function<void(Bullet&)>& visitor);
    /// @brief 碰撞与绘制读取的演员状态；position 每次 Update 后与 Position() 一致。
    bool active=false;
    bool phaseClockPaused=false; // 只冻结阶段超时钟；局部协程、移动、自动发射及动画继续，Reset 才无条件解除。
    VisualId visual=kInvalidVisualId,defeatVisual=kInvalidVisualId;
    Vector3 position{},origin{};
    float age=0,hitRadius=0.55F;
    int scoreValue=0,sourceItem=-2;
    ActorPresentation presentation;
    battle::Cursor animation; // 借用当前关卡 ANM，清池必须先于关卡资源卸载。
    std::optional<battle::AnimationInterrupt> animationInterrupt;
    /// @brief 返回受伤开关；战斗调用方先筛选 active，免伤不会关闭本体接触。
    [[nodiscard]] bool CanTakeDamage() const { return damageable&&!retiring&&interactable&&visible; }
    /// @brief 自机弹和追踪可接触的活动本体；受伤保护不关闭弹接触，隐藏/非交互演员不吸收玩家弹。
    [[nodiscard]] bool CanBeTargeted() const { return active&&!retiring&&interactable&&visible; }
    /// @brief 活动且生命为正的本体参与接触；视觉淡出和受伤开关不改变此条件。
    [[nodiscard]] bool CanContactPlayer() const { return CanBeTargeted()&&contactEnabled&&health>0; }
    /// @brief 返回实际技能发射点的世界坐标，供判定调试显示；不改变行为状态。
    [[nodiscard]] Vector3 MuzzlePosition() const {
        return {motion.position.x+skills.offset.x,motion.position.y+skills.offset.y,motion.position.z+skills.offset.z};
    }
    movement::State motion;
    std::optional<movement::LiftedPath> liftedMotion; // 可选等路程空间延拓；拥有平面编排状态，跨阶段保留，Reset 清空。
    std::uint32_t spatialIndex=1; // 独立高度采样序号，来自固定出生种子，不消耗攻击/分支随机源。
    std::uint32_t spawnIndex=1; // 子演员独立种子的递增序号；Reset 固定初值，不消耗攻击随机流或空间采样序号。
    skill::State skills;
    float health=1,maxHealth=1,lifeThreshold=-1; // 原始 HP，阈值为负关闭；生命回调优先于击破回调。
    bool damageable=true,finished=false,retiring=false;
    bool interactable=true; // 瞬移/隐藏窗口可同时关闭受击和接触；不修改可受伤开关或生命。
    bool visible=true,contactEnabled=true; // 隐藏保留行为/槽位；关闭本体接触仍允许玩家弹命中，与伤害保护分开。
    bool rotateAnimation=false;
    Entry pendingCallback=nullptr,lifeCallback=nullptr,deathCallback=nullptr,timeoutCallback=nullptr;
    CallbackCause callbackCause=CallbackCause::None;
    double phaseFrames=0; // 60 Hz 阶段内容帧；120 Hz 更新时每步增加 0.5。
    int timeoutFrames=-1,deathEffect=0; // timeoutFrames 为 60 Hz 内容帧，负值关闭超时。
    std::vector<int> soundEvents,dropEvents,itemEvents; // 本次 Update 的音效/散落数量/原生道具号，下次 Update 清空，调用方只即时借用。
    std::vector<Spawn> spawnEvents; // 本次 Update 请求的独立子演员；接收方复制后生成，不在 ActorPool 遍历中重入出怪。
    bool defeatActors=false; // 本次 Update 请求击破场上普通演员；不清 Boss，由总控走正常奖励/死亡回调。
    int animationScript=-1;
    double animationAge=0;
    std::uint32_t animationRevision=0;
    std::array<int,5> poses{-1,-1,-1,-1,-1}; // 图集内待机、左收势、右收势、左移、右移；负值关闭自动姿态。
private:
    friend struct Routine::Awaiter;
    void Attach(Routine::Handle handle);
    void ClearRoutines();
    void Tick();
    std::vector<Routine::Handle> stack_;
    std::uint32_t random_=1;
    const int updateHz_;
    double remainingTicks_=0; // 未消费的更新步数，按 updateHz_ 累计，阶段交接时保留。
    int pose_=-1;
    Vector3 player_{};
    BulletSystem* bullets_=nullptr;
    bool entered_=false,retireOutside_=false;
    movement::Bounds retirementBounds_;
    std::optional<Vector2> retirementHeights_;
    VisualId defeatBase_=kInvalidVisualId;
    std::array<BulletSystem::Handle,16> beams_{}; // 跨阶段借用同一弹池，代号保护复用；不拥有光束或其动画资源。
    skill::Shot emitterShot_;
    skill::State emitterSkills_;
    int emitterPeriod_=0;
    double emitterElapsed_=0;
    bool emitterConfigured_=false;
    int emitterSound_=-1; // 音效身份跨配方保留，是否播放由各次发射的 PlaySound 位决定。
    std::uint64_t animationInterruptSequence_=0;
};
/// @brief 将通过 Valid 的独立路径、动画与单次技能按局部帧合并；相同时刻先移动变更，再发弹。
Routine Follow(Actor& actor,Pattern pattern);
/// @brief 检查路径、技能、动画帧及发弹偏移的可用性，不修改配置。
[[nodiscard]] bool Valid(const Pattern& pattern);
/// @brief 检查世界位置、X/Z 与可选 Y 边界、随机范围、战斗及行为配置；素材编号仅检查本体非 invalid。
[[nodiscard]] bool Valid(const Spawn& spawn);

/// @brief 固定槽位持有 behavior::Actor，不使用继承或关卡工厂；容量耗尽只丢弃该次生成请求。
template<std::size_t Capacity>
class ActorPool {
public:
    /// @brief 直接按配置初始化独立 Actor；返回借用指针，槽位复用/Clear 后旧引用失效。
    /// 非法配置抛 invalid_argument，池满返回 nullptr 并增加丢弃数。
    Actor* Spawn(const behavior::Spawn& spawn) {
        if(!Valid(spawn))throw std::invalid_argument("Invalid behavior actor spawn");
        for(auto& slot:actors_)if(!slot||!slot->active) {
            if(!slot)slot=std::make_unique<Actor>();
            slot->Activate(spawn);return slot.get();
        }
        ++dropped_;return nullptr;
    }
    /// @brief 对活动演员调用 Function(Actor&)，调用期间不得清池或向池内生成新演员。
    template<class Function> void ForEachActive(Function&& function) {
        for(auto& slot:actors_)if(slot&&slot->active)function(*slot);
    }
    /// @brief 以只读引用访问活动演员，回调不得保留跨槽位复用的引用。
    template<class Function> void ForEachActive(Function&& function) const {
        for(const auto& slot:actors_)if(slot&&slot->active)function(static_cast<const Actor&>(*slot));
    }
    /// @brief 销毁全部演员及挂起协程，清零丢弃数；须先于卸载关卡资源。
    void Clear() { for(auto& slot:actors_)slot.reset();dropped_=0; }
    /// @brief 返回自上次 Clear 后因容量耗尽而丢弃的生成请求数，不包括非法配置。
    [[nodiscard]] std::size_t DroppedSpawns() const { return dropped_; }
private:
    std::array<std::unique_ptr<Actor>,Capacity> actors_{};
    std::size_t dropped_=0;
};
} // namespace koumakyou::behavior
