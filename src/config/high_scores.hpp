// 本文件定义按标准/挑战、四御札和难度分组的前十成绩，以及独立于设置的结局与命名存档。
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace koumakyou::score {
inline constexpr char kFilename[]="koumakyou_scores.dat";
inline constexpr int kSlots=10,kNameLength=8,kShots=4,kDifficulties=5;
/// @brief 完成时间为 Unix 秒；value 为原始非负 64 位分数，name 为 1～8 个可打印 ASCII 字符。
/// misses/bombs/graze 为本轮累计值；旧版 best_score 不含完整本局身份，不能自动迁入此表。
struct Entry {
    long long value=0;
    std::string name="PLAYER";
    int misses=0,bombs=0,graze=0;
    std::int64_t completedAt=0;
};
using Table=std::array<std::optional<Entry>,kSlots>;
/// @brief groups 按 [标准=0/挑战=1][灵/梦/魔/恋=0～3][Easy～Extra=0～4] 保存，分数降序、空槽在后。
/// 全部字符串和记录按值独占；同分新成绩排在旧成绩之前，与原作排名插入规则一致。
struct Profile {
    std::array<std::array<std::array<Table,kDifficulties>,kShots>,2> groups{};
    std::array<bool,6> endingsSeen{};
    std::string lastName="PLAYER";
    /// @brief 无效组返回借用的空表；引用随 Profile 赋值/销毁失效。
    [[nodiscard]] const Table& Get(bool challenge,int shot,int difficulty=1) const;
    /// @brief 返回待插入的 0～9 排名；无效组、负分或未进入前十返回空，不改变记录。
    [[nodiscard]] std::optional<int> Position(bool challenge,int shot,int difficulty,long long value) const;
    /// @brief 仅在身份、名字和计数均有效且进入前十时插入；成功后末位移出，失败不改变任何记录。
    bool Insert(bool challenge,int shot,int difficulty,const Entry& entry);
};
/// @brief 检查原作八字符命名约束，不接受控制字符、非 ASCII 或全空格名字。
[[nodiscard]] bool ValidName(std::string_view name);
/// @brief 缺失或损坏行使用默认值；读取不重写文件，不把旧设置或原作 score.dat 当成本存档。
[[nodiscard]] Profile Load(const std::filesystem::path& path);
/// @brief 先写同目录临时文件再替换；创建/写入/替换失败返回 false，保留原文件，调用方应保留待保存内容。
[[nodiscard]] bool Save(const std::filesystem::path& path,const Profile& profile);
} // namespace koumakyou::score
