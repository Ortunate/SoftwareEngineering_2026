// 本文件定义新典七关生命记录与四机体火力的只读运行时目录，所有生命和伤害保持原始单位。
#pragma once
#include "raylib.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace koumakyou {
/// @brief 四机体的稳定编号，与成绩存档及原始火力表身份一致。
enum class ShotType { HomingAmulet, Needle, MagicMissile, IllusionLaser };
namespace native_combat {
inline constexpr int kFramesPerSecond=60, kShotWindowFrames=30;
/// @brief 四种主线难度的原始 ECL 位掩码；UI 和存档索引另为 0～3。
enum class Difficulty : std::uint16_t { Easy=0x100, Normal=0x200, Hard=0x400, Lunatic=0x800 };
/// @brief 将 ECL 掩码身份转换为 E/N/H/L 的 0～3 索引；非法枚举返回 -1。
[[nodiscard]] constexpr int DifficultyIndex(Difficulty difficulty) {
    switch(difficulty) {case Difficulty::Easy:return 0;case Difficulty::Normal:return 1;
        case Difficulty::Hard:return 2;case Difficulty::Lunatic:return 3;}
    return -1;
}
/// @brief 将 E/N/H/L 的 0～3 索引转换为掩码身份；其他索引返回空，不包含 Extra。
[[nodiscard]] constexpr std::optional<Difficulty> DifficultyFromIndex(int index) {
    switch(index) {case 0:return Difficulty::Easy;case 1:return Difficulty::Normal;
        case 2:return Difficulty::Hard;case 3:return Difficulty::Lunatic;default:return std::nullopt;}
}
/// @brief DataOnly 表示仅注册数值，尚无可玩关卡；不得据此开放关卡选择或构造演员。
enum class Integration { DataOnly, Playable };
enum class HealthScope { Instruction, TimelineSpawn, SubSpawn };
enum class HealthKind { DamageEnabled, DeathCallback, SetHealth, ResetTimer, Threshold, HealthCallback, Timeout, TimeoutCallback, Spawn, DefaultSpawn };
/// @brief 一条原始生命设置或出生记录；frame 在时间轴为关卡帧，在子程序为局部脚本帧。
/// value 缺失代表引擎默认值，variable=true 代表上下文寄存器；两者都不能直接用作常量生命。
/// normalIndex 是筛选 Normal 后的指令序号；非 Normal 指令及时间轴为 -1。位置是原作逻辑像素。
struct HealthRecord {
    HealthScope scope;
    int sub, timeline, frame, opcode, normalIndex;
    std::uint32_t sourceOffset;
    std::uint16_t rankMask, parameterMask;
    HealthKind kind;
    std::optional<int> value;
    bool variable;
    int targetSub;
    Vector3 position;
    int item, score;
};
/// @brief stage 为 1～7（7 是 Extra），记录和哈希均借用程序静态存储，调用方不得释放。
struct StageData { int stage; Integration integration; std::string_view sha256; std::span<const HealthRecord> health; };
/// @brief 原始发射通道；period/phase 是 60 Hz 周期/相位，kind=3 时改为独立槽冷却/槽号。
/// module=0 主机、1/2 左右副机；kind=0 直射、1 追踪、2 导弹、3 激光。
/// x/y/width/height 是逻辑像素（Y 向下），角为弧度，speed 为像素/帧；damage 为原始点数，激光按有效接触帧计。
struct ShotChannel {
    int period, phase, module, kind, script, sound;
    float x,y,width,height,angle,speed,damage;
    std::uint32_t sourceRva;
};
/// @brief 两端包含的 Power 区间，覆盖 0～128；shots 借用静态存储，原始记录顺序不可改为齐射分组。
struct ShotLevel { int minimumPower, maximumPower; std::span<const ShotChannel> shots; };
/// @brief 机体火力表及其 PE RVA；name 和 levels 借用程序静态存储，RVA 只在 ExecutableHash 对应版本有效。
struct ShotDefinition { ShotType id; std::string_view name; std::uint32_t tableRva; std::span<const ShotLevel> levels; };
/// @brief 返回所有已导入数值，不意味着关卡已实现；span 与记录均具有程序生命周期。
std::span<const StageData> Stages();
std::span<const ShotDefinition> Shots();
/// @brief 未导入关卡返回 nullptr；stage 采用 1～7 的原作编号，与可玩 StageId 分离。
const StageData* Stage(int stage);
/// @brief 按 Power 0～128 选档，越界钳制；非法机体抛 invalid_argument。
const ShotLevel& ForPower(ShotType shot,int power);
/// @brief 查询一条 Normal 解释器生命指令；无匹配返回 nullptr，调用方不可将缺失值视为零。
const HealthRecord* Instruction(int stage,int sub,int normalIndex);
/// @brief 查询指定难度的局部帧设置；多条匹配或无匹配返回 nullptr，适用于唯一的阶段初始化常量。
const HealthRecord* Setting(int stage,int sub,int opcode,int frame=0,Difficulty difficulty=Difficulty::Normal);
/// @brief 查询时间轴唯一出生记录；无匹配或重复返回 nullptr。
const HealthRecord* Spawn(int stage,int timeline,int frame,int targetSub);
/// @brief 返回常量值；缺失、动态寄存器引用或未知记录抛 logic_error，防止假装已经实现默认规则。
int RequireLiteral(const HealthRecord* record);
std::string_view ExecutableHash();
} // namespace native_combat
} // namespace koumakyou
