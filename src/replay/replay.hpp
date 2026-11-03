// 本文件定义录像的开局配置、逐步输入和有界文件读写接口。
#pragma once
#include "gameplay/actors/player.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace koumakyou::replay {
/// @brief 模式仅属于本次会话，不写回玩家设置。
enum class Mode { Idle, Recording, Playback };
/// @brief 协议难度与运行时菜单下标独立；Extra 仅用于独立追加路线。
enum class Difficulty : std::uint32_t { Easy, Normal, Hard, Lunatic, Extra };
/// @brief 会话开局；shot 为四机体 0～3，phase 为符卡目录的遭遇内展开技能编号，种子在首次掉落初始化前设置。
struct Header {
    std::uint32_t seed=1, shot=0, lives=2, bombs=3, phase=2;
    bool practice=false, spell=false, challenge=false;
};
/// @brief 一次逻辑更新；dt 单位秒，范围 (0,0.05]，不包含暂停或加载时间；confirm 合并键盘和鼠标确认，player.aimAssistEnabled 固定本步弹道辅助设置。
struct Frame { float dt=0; PlayerInput player{}; bool confirm=false, fast=false; };
/// @brief 终局摘要在结束步骤全部执行后采样，不含退场动画；score 非负，lives包含当前自机，misses/bombs为本局累计使用次数。
struct Result {
    std::uint64_t score=0;
    std::uint32_t lives=0,power=8,misses=0,bombs=0;
    bool won=false;
    bool operator==(const Result&) const = default;
};
/// @brief 关入口快照；score为前面累计分数，lives为额外残机数（不含当前自机），bombs为持有量，不能序列化资源指针。
struct Segment {
    std::string stage="stage1", entry="stage", target;
    Difficulty difficulty=Difficulty::Normal;
    std::uint32_t begin=0,end=0,seed=1,lives=2,bombs=3,power=8,rank=16;
    std::uint64_t score=0;
    Result result;
};
/// @brief 拥有完整录像数据；上限避免损坏文件或无限运行耗尽内存。
struct Data {
    Header header; std::vector<Frame> frames; Result result;
    std::string name="Replay", route="stage1-prototype", character="reimu", shot="reimu-a", rules="standard";
    std::string simulation,content,build;
    /// @brief 开局时刻，UTC Unix 秒；旧录像未记录时为 0，不用当前时间或文件修改时间补造。
    std::uint64_t recordedAt=0;
    std::uint32_t continues=0;
    std::vector<Segment> segments;
};
/// @brief 新会话/片段生命周期；结束片段只固定输入边界，不提交磁盘。非法顺序抛 invalid_argument。
void BeginSession(Data& data,const Header& header);
void BeginStage(Data& data,Segment segment);
void EndStage(Data& data,const Result& result);
void FinishSession(Data& data,const Result& result);
/// @brief 校验 UTF-8 名称，至多 32 个 Unicode 标量且不含控制字符；失败不修改输出。
bool NormalizeName(const std::string& input,std::string& output);
/// @brief 校验整轮主线、Extra、单关练习或单符练习的入口及顺序；未知组合返回错误，绝不回退。
bool CanPlay(const Data& data,std::string& error);
/// @brief 已解析的内容身份；stage=0…6（6为Extra），difficulty=0…4，spellNumber=0表示整关。
struct Entry { int stage=0,difficulty=1,spellNumber=0,group=0,phase=2; };
/// @brief 解析一个片段的关卡和符卡目录身份；不检查本机解锁记录，非法身份返回false。
bool ResolveEntry(const Segment& segment,Entry& entry);
inline constexpr std::size_t kMaxFrames=2'000'000;
/// @brief 同一源码、构建类型、编译器和平台的兼容标识，借用静态字符串。
const char* BuildId();
/// @brief 模拟和内容版本分别用于播放资格，显示和镜头改动不计入模拟版本。
const char* SimulationId();
const char* ContentId();
/// @brief 原子发布唯一文件，失败返回 false 和中文错误；不覆盖已有录像。
bool Save(const std::filesystem::path& directory,const Data& data,std::filesystem::path& saved,std::string& error);
/// @brief 完整校验后替换 out；失败保持 out 不变，并返回中文错误。
bool Load(const std::filesystem::path& file,Data& out,std::string& error);
}
