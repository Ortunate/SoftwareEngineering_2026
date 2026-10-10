// 本文件保存新典 msg1 与 zh-CN 翻译表中的灵梦战前/战后原文、等待时长和入场顺序。
#include "stages/stage1/data.hpp"
#include <array>
namespace koumakyou::stage1 {
namespace {
constexpr std::array<DialogueLine,15> kBefore{{
    {"灵梦", "好久没开工啦", false, 6.0F, 0.0F, false, false, "感觉真惬意", 0, 1.0F},
    {"灵梦", "每次白天出门都见不到几个恶灵", false, 6.016666666666667F, 0.0F, false, false, "所以今天改成了晚上出发...", 0, 0.0F},
    {"灵梦", "可这黑灯瞎火的", false, 5.35F, 0.3333333333333333F, false, false, "根本搞不清该往哪儿走呀", 5, 0.0F},
    {"灵梦", "不过...", false, 2.5166666666666666F, 0.0F, false, false, "", 5, 0.0F},
    {"灵梦", "神社后院到了晚上还挺浪漫嘛", false, 4.0F, 0.0F, false, false, "（←悠哉）", 3, 0.0F},
    {"露米娅", "可不是嘛~", true, 5.183333333333334F, 0.16666666666666666F, true, false, "而且还会闹鬼，简直太棒了", 1, 0.0F},
    {"灵梦", "等等，", false, 5.516666666666667F, 0.0F, true, true, "你谁呀？", 5, 0.0F},
    {"露米娅", "我们刚才不是见过面吗", true, 5.016666666666667F, 0.0F, true, false, "难不成，你是雀盲眼？", 0, 0.0F},
    {"灵梦", "人在黑暗之中", false, 5.016666666666667F, 0.0F, true, false, "本来就看不清东西啦", 0, 0.0F},
    {"露米娅", "咦？可我好像也见过", true, 5.016666666666667F, 0.0F, true, false, "只在夜间活动的人呀", 3, 0.0F},
    {"灵梦", "那种人可以直接抓来吃哦", false, 5.016666666666667F, 0.0F, true, false, "", 3, 0.0F},
    {"露米娅", "是—这样吗—", true, 5.016666666666667F, 0.0F, true, false, "", 2, 0.0F},
    {"灵梦", "话说，你挡我道了", false, 5.016666666666667F, 0.0F, true, false, "", 0, 0.0F},
    {"露米娅", "眼前这个人类就能抓来吃？", true, 5.016666666666667F, 0.0F, true, false, "", 2, 0.0F},
    {"灵梦", "有句话叫良药苦于口", false, 8.85F, 0.0F, true, false, "你不知道吗？", 3, 0.0F},
}};
constexpr std::array<DialogueLine,2> kAfter{{
    {"灵梦", "但是不是良药", false, 6.016666666666667F, 0, false, false, "", 0},
    {"灵梦", "只有喝了才知道哦", false, 15.5F, 0, false, false, "", 0},
}};
}
std::span<const DialogueLine> Stage1Dialogue(bool afterBattle) { return afterBattle ? std::span<const DialogueLine>(kAfter) : std::span<const DialogueLine>(kBefore); }
} // namespace koumakyou::stage1
