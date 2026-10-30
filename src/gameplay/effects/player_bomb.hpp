// 本文件定义四种新典 Bomb 的固定 60 Hz 攻击、清弹和三维演出实例，不拥有目标或图形资源。
#pragma once
#include "gameplay/data/native_combat.hpp"
#include "gameplay/data/native_sizes.hpp"
#include "gameplay/effects/bomb_volume.hpp"
#include "resources/battle_catalog.hpp"
#include <array>
#include <functional>
namespace koumakyou {
/// @brief 帧数均为原作 60 Hz；脚本编号属于 atlas，回调 RVA 对应 native_combat::ExecutableHash。
struct PlayerBombDefinition {
    ShotType shot;
    std::string_view name;
    battle::Atlas atlas;
    int firstScript, layers, durationFrames, invulnerabilityFrames;
    std::uint32_t calcRva, drawRva;
};
/// @brief 返回四种御札各自的 Bomb，编号与 ShotType 相同；非法机体抛 invalid_argument。
[[nodiscard]] const PlayerBombDefinition& BombDefinition(ShotType shot);
/// @brief Bomb 的原生尺寸、运动与清弹容差沿用全战斗统一的世界/逻辑像素比例。
inline constexpr float kBombWorldPixel=native_sizes::kWorldPerPixel;
inline constexpr float kBombCancelMargin=20*kBombWorldPixel;
/// @brief 魔札将场地按每轴 4 格铺成 64 个等体积区域；180 帧抵达格心并保持至结束。
/// 每颗星沿真实路径攻击，半径仍为原作 64 像素；体积覆盖率不计清弹容差或目标半径。
inline constexpr int kBombStarGrid=4,kBombStarCount=64,kBombStarTravelFrames=180;
/// @brief 当前逻辑帧的攻击实体，damage 为原始 HP；region 是本次 Bomb 内的通道编号 0～7。
/// 同一 region 的多个实体取并集，每个目标每帧只计一次；不能按纹理层数重复扣血。
struct PlayerBombAttack { BombVolume volume; int region=0,damage=0; };
/// @brief 三维演出快照；volume 同时用于攻击和渲染，position/end/height/width 均为世界单位。
/// layer 属于对应 Bomb 的素材数组；frame 为独立 ANM 采样，绘制不能推进动画或改变攻击几何。
struct PlayerBombSprite {
    enum class Shape { Sphere, Slab, Star, Beam } shape=Shape::Sphere;
    int layer=0;
    Vector3 position{},end{}; // 模型外观锚点；光炮炮口可偏移，攻击判定始终使用独立 volume。
    float height=1,width=0,ageSeconds=0;
    bool trail=false; // 拖影仅绘制光晕，不重复生成实体星芒或伤害。
    BombVolume volume{};
    anm::Frame frame{};
};
/// @brief 持有七颗光球、六面符阵、64 颗星及拖影或同轴光束，独立于普通弹幕池容量。
/// ANM 借用当前关卡，卸载前必须 Clear；允许无档案运行攻击模拟，缺少素材不能让 Bomb 失去伤害。
class PlayerBombEffect {
public:
    /// @brief 启动对应攻击；seed 决定封印初速度。档案可为空，非空时必须含全部脚本，否则抛异常。
    /// 相机必须有有效正交基；调用替换旧实例，不保留目标指针。
    void Start(ShotType shot,const Camera3D& camera,const anm::Archive* archive=nullptr,std::uint32_t seed=1);
    void Start(ShotType shot,const Camera3D& camera,const anm::Archive& archive,std::uint32_t seed=1) { Start(shot,camera,&archive,seed); }
    /// @brief 推进非负有限秒数，逐个补齐 60 Hz 帧；每帧构造攻击后立即 resolve，低 FPS 不丢命中或爆破。
    /// targets 借用当前可受伤目标位置。resolve 可 ReportHit 或 Clear，但不得重入 Start/Update。
    /// 暂停时不调用；seconds=0 只刷新恋札朝向，不产生伤害帧。
    void Update(float seconds,const Camera3D& camera,std::span<const Vector3> targets={},
                const std::function<void(PlayerBombEffect&)>& resolve={});
    /// @brief 累计实际接触的原始伤害；仅封印的 1～7 通道有效，达到 100 后下帧爆破。
    /// 必须在本帧 resolve 中、每个目标/region 至多调用一次；无敌或已死亡目标不记录。
    void ReportHit(int region,int damage);
    /// @brief 清除攻击、音效和快照并释放 ANM 借用，不销毁档案。
    void Clear();
    [[nodiscard]] bool Active() const { return active_; }
    [[nodiscard]] ShotType Shot() const { return shot_; }
    [[nodiscard]] int Frame() const { return frame_; }
    [[nodiscard]] std::span<const PlayerBombSprite> Sprites() const { return {sprites_.data(),spriteCount_}; }
    [[nodiscard]] std::span<const PlayerBombAttack> Attacks() const { return {attacks_.data(),attackCount_}; }
    /// @brief 查询当前逻辑帧与目标球接触的八个通道原始 HP，同通道实体取最大值，未接触返回全零。
    /// center/radius 为世界坐标/非负世界半径；不计纹理柔边、拖影或清弹容差，不反馈光球累计。
    /// 调用方仅对可受伤且存活目标求和结算，并在本帧 resolve 中逐通道 ReportHit 一次。
    [[nodiscard]] std::array<int,8> ContactDamage(Vector3 center,float radius) const;
    /// @brief 当前帧清弹实体；梦札蓄力后每帧清弹，魔/恋札分别跳过 3/4 的倍数帧。
    /// 封印初始保护只在 0 帧，光球爆破只在触发帧清弹；视觉残留不延长实体寿命。
    [[nodiscard]] std::span<const BombVolume> CancelVolumes() const { return {cancelVolumes_.data(),cancelCount_}; }
    /// @brief 最近一次 Start/Update 的音效编号，借用至下次更新；调用方同帧合并相同编号。
    [[nodiscard]] std::span<const int> SoundEvents() const { return {sounds_.data(),soundCount_}; }
    /// @brief 60 帧渐入/渐出背景压暗，通常 Alpha 0～176；恋札为保留束内视野使用 0～96。
    [[nodiscard]] unsigned char BackdropAlpha() const;
private:
    void Tick(const Camera3D& camera,std::span<const Vector3> targets);
    void Sample(const Camera3D& camera);
    void StartOrb(int index,const Camera3D& camera);
    bool active_=false;
    const anm::Archive* archive_=nullptr;
    ShotType shot_=ShotType::HomingAmulet;
    int frame_=0;
    double fractionalFrames_=0;
    std::uint32_t random_=1;
    Vector3 anchor_{},forward_{},right_{},up_{};
    std::array<anm::Player,kBombStarCount> animations_{};
    std::array<Vector3,kBombStarCount> positions_{},velocities_{};
    std::array<int,8> orbBorn_{},orbBurst_{},orbDamage_{}; // 0 号为空槽；-1 表示未出生/未爆破。
    // 64 颗星各保留一个核心和一个拖影；伤害通道仍为 8 个，增加覆盖不会线性放大叠加伤害。
    std::array<PlayerBombSprite,kBombStarCount*2> sprites_{};
    std::array<PlayerBombAttack,kBombStarCount> attacks_{};
    std::array<BombVolume,kBombStarCount> cancelVolumes_{};
    std::size_t spriteCount_=0,attackCount_=0,cancelCount_=0;
    std::array<int,16> sounds_{};
    std::size_t soundCount_=0;
};
} // namespace koumakyou
