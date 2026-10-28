// 本文件定义符卡练习目录、角色组合、层级选择与独立成绩；原典编号不依赖当前战斗实现。
#pragma once
#include <array>
#include <span>
#include <string_view>
#include <algorithm>

namespace koumakyou::spell {
inline constexpr int kSpellCount=131, kCharacterCount=4, kStageCount=7;
/// @brief 原典关卡从 0 编号，6 表示 Extra；groups 是该关符卡组数，total 包含所有难度变体。
struct Stage { const char* label; const char* title; const char* boss; int groups, total; };
/// @brief 四种角色/御札组合；portrait=0 为灵梦、1 为魔理沙，stars 依次为速度、范围和攻击力。
struct Character { const char* name; const char* title; const char* displayName; const char* shot; const char* bomb;
    int portrait; std::array<int,3> stars; };
/// @brief number 为原典 1～131 编号；stage/group 为从 0 开始的关内编号，difficulty=0～4 对应 E/N/H/L/EX。
/// phase 为本项目已实现的 Boss 展开技能编号（2/4），对应运行时 attack，0 表示尚未实现，不能据此启动关卡。
struct Card { int number,stage,group,difficulty; const char* name; const char* user; int phase; };
/// @brief 返回七关只读目录；借用静态数据，不加载素材。
[[nodiscard]] std::span<const Stage> Stages();
/// @brief 返回灵/梦/魔/恋四组合；借用静态原文与能力评级。
[[nodiscard]] std::span<const Character> Characters();
/// @brief 返回按原典编号排序的 131 个难度变体；借用静态数据。
[[nodiscard]] std::span<const Card> Cards();
/// @brief 查询关内符卡组与难度；不存在的难度返回 nullptr，不允许把空槽映射为其他符卡。
[[nodiscard]] const Card* Find(int stage,int group,int difficulty);
/// @brief 只检查战斗实现：四种御札均支持 Normal 夜符/暗符；实际练习入口须使用 Available 追加遇见条件。
[[nodiscard]] bool Playable(const Card* card,int character);
/// @brief 返回关内符卡组的使用者原文；无此组返回空串，字符串具有静态生命周期。
[[nodiscard]] const char* GroupUser(int stage,int group);
inline constexpr std::array<const char*,5> kDifficulties{"EASY","NORMAL","HARD","LUNATIC","EXTRA"};
/// @brief Stage 为关卡页，Group/Difficulty 为同一选卡页的两级操作焦点；切换焦点不能重新播放页面进场。
/// 横向输入独立循环四种角色，纵向输入改变当前列；鼠标移动可直接切换列，键盘确认/返回逐级切换。
enum class Page { Stage, Group, Difficulty };
struct Selection { Page page=Page::Stage; int stage=0,group=1,difficulty=1,character=0; };
/// @brief 单张符卡的本机记录；mainAttempts>0 说明已在实际关卡内遇见，不要求击破。
/// 计数上限 20 亿，bestScore 为非负 64 位分数，不能导入原游戏的个人成绩。
struct Record { long long bestScore=0; int attempts=0,captured=0,mainAttempts=0,mainCaptured=0; };
/// @brief 第一维角色组合，第二维原典符卡编号减一；预览和调试靶场不写入，由 Game 管理统计生命周期。
struct Records {
    std::array<std::array<Record,kSpellCount>,kCharacterCount> values{};
    /// @brief 失败返回空只读记录；引用由本结构持有，不转移所有权。
    [[nodiscard]] const Record& Get(int character,int number) const {
        static const Record empty{};
        return character>=0 && character<kCharacterCount && number>=1 && number<=kSpellCount ? values[character][number-1] : empty;
    }
    /// @brief character 为灵/梦/魔/恋的 0～3，number 为原典符卡编号 1～131；无效参数返回 false。
    /// 必须由当前组合在普通开局或整关练习遇见；其他组合、难度变体、练习成绩和整关分数均不能代为解锁。
    [[nodiscard]] bool Seen(int character,int number) const {
        return Get(character,number).mainAttempts>0;
    }
    /// @brief 开始一次有效尝试，practice 区分练习与本篇；调用方每个阶段只能调用一次。
    void Begin(int character,int number,bool practice) {
        if(character<0 || character>=kCharacterCount || number<1 || number>kSpellCount)return;
        auto& r=values[character][number-1];auto& count=practice?r.attempts:r.mainAttempts;
        if(count<2000000000)++count;
    }
    /// @brief 结束尝试；仅练习提升最高分，本篇只更新取得数；capture 必须排除 Miss、Bomb 和超时，调用方保证每次尝试只结算一次。
    void Finish(int character,int number,bool practice,bool capture,long long score) {
        if(character<0 || character>=kCharacterCount || number<1 || number>kSpellCount)return;
        auto& r=values[character][number-1];if(practice)r.bestScore=std::max(r.bestScore,score);
        auto& captured=practice?r.captured:r.mainCaptured;
        const int attempts=practice?r.attempts:r.mainAttempts;
        if(capture && captured<attempts)++captured;
    }
};
/// @brief 练习启动与菜单亮色的共同条件：当前角色/御札组合已实现该符卡，并以该组合遇见过相同难度编号。
[[nodiscard]] inline bool Available(const Card* card,int character,const Records& records) {
    return Playable(card,character) && records.Seen(character,card->number);
}
} // namespace koumakyou::spell
