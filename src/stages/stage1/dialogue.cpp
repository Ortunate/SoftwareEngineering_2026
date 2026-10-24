// 本文件保存新典 msg1 与 zh-CN 翻译表中两位自机的战前/战后原文、等待时长和入场顺序。
#include "stages/stage1/data.hpp"
#include <array>
namespace koumakyou::stage1 {
namespace {
constexpr std::array<DialogueLine,15> kBefore{{
    {"灵梦", "好久没开工啦", false, 361.0F/60, 0.0F, false, false, "感觉真惬意", 0, 1.0F, 0, 68.0F/60, -1, true},
    {"灵梦", "每次白天出门都见不到几个恶灵", false, 6.016666666666667F, 0.0F, false, false, "所以今天改成了晚上出发...", 0, 0.0F},
    {"灵梦", "可这黑灯瞎火的", false, 321.0F/60, 20.0F/60, false, false, "根本搞不清该往哪儿走呀", 5, 0, 20.0F/60, 28.0F/60},
    {"灵梦", "不过...", false, 2.5166666666666666F, 0.0F, false, false, "", 5, 0.0F},
    {"灵梦", "神社后院到了晚上还挺浪漫嘛", false, 4.0F, 0.0F, false, false, "（←悠哉）", 3, 0.0F},
    {"露米娅", "可不是嘛~", true, 311.0F/60, 10.0F/60, true, false, "而且还会闹鬼，简直太棒了", 1, 0, 0, 18.0F/60},
    {"灵梦", "等等，", false, 301.0F/60, 0.0F, true, true, "你谁呀？", 5, 0, 0, 30.0F/60},
    {"露米娅", "我们刚才不是见过面吗", true, 5.016666666666667F, 0.0F, true, false, "难不成，你是雀盲眼？", 0, 0.0F},
    {"灵梦", "人在黑暗之中", false, 5.016666666666667F, 0.0F, true, false, "本来就看不清东西啦", 0, 0.0F},
    {"露米娅", "咦？可我好像也见过", true, 5.016666666666667F, 0.0F, true, false, "只在夜间活动的人呀", 3, 0.0F},
    {"灵梦", "那种人可以直接抓来吃哦", false, 5.016666666666667F, 0.0F, true, false, "", 3, 0.0F},
    {"露米娅", "是—这样吗—", true, 5.016666666666667F, 0.0F, true, false, "", 2, 0.0F},
    {"灵梦", "话说，你挡我道了", false, 5.016666666666667F, 0.0F, true, false, "", 0, 0.0F},
    {"露米娅", "眼前这个人类就能抓来吃？", true, 5.016666666666667F, 0.0F, true, false, "", 2, 0.0F},
    {"灵梦", "有句话叫良药苦于口", false, 530.0F/60, 0.0F, true, false, "你不知道吗？", 3, 0, 0, 8.0F/60, 500.0F/60},
}};
constexpr std::array<DialogueLine,2> kAfter{{
    {"灵梦", "但是不是良药", false, 360.0F/60, 0, false, false, "", 2, 1.0F, 0, 68.0F/60},
    {"灵梦", "只有喝了才知道哦", false, 960.0F/60, 0, false, false, "", 2, 0, 0, 8.0F/60, 900.0F/60},
}};
// msg1.dat 的第 10/11 段属于魔理沙；两种御札共用同一对话，索引 ST_MSG1_17…28。
constexpr std::array<DialogueLine,10> kMarisaBefore{{
    {"魔理沙","这种感觉，该怎么形容呢...",false,361.0F/60,0,false,false,"",0,1.0F,0,68.0F/60},
    {"魔理沙","换做是那家伙，肯定会说什么",false,301.0F/60,0,false,false,"「感觉真惬意」吧",0},
    {"魔理沙","但我可不喜欢夜晚",false,361.0F/60,0,false,false,"",1},
    {"魔理沙","路上碰到的全是怪胎",false,340.0F/60,0,false,false,"",1},
    {"露米娅","你说谁是怪胎呢",true,201.0F/60,0,true,false,"",3},
    {"魔理沙","我可没说你",false,301.0F/60,0,true,true,"",3,0,0,30.0F/60},
    {"露米娅","这个嘛，我当然不是",true,301.0F/60,0,true,false,"",0},
    {"魔理沙","所以，你把手伸这么开干嘛",false,301.0F/60,0,true,false,"",0},
    {"露米娅","这样看起来像不像",true,301.0F/60,0,true,false,"「圣人被钉在十字架上」？",2},
    {"魔理沙","我看更像是",false,430.0F/60,0,true,false,"「人类采用了十进制」",3,0,0,8.0F/60,400.0F/60},
}};
constexpr std::array<DialogueLine,2> kMarisaAfter{{
    {"魔理沙","难道说",false,360.0F/60,0,false,false,"",1,1.0F,0,68.0F/60},
    {"魔理沙","只有人类是十根手指吗",false,560.0F/60,0,false,false,"",1,0,0,8.0F/60,500.0F/60},
}};
// 时间来自 msg1.dat 的 0/1/10/11 段，包含冻结脚本时钟的 wait；同帧指令顺序不得重排。
constexpr DialogueCue P(int frame,int slot,int label) { return {frame,DialogueCueKind::Portrait,slot,label}; }
constexpr DialogueCue I(int frame,bool enabled) { return {frame,DialogueCueKind::InputEnabled,0,enabled}; }
constexpr DialogueCue M(int frame) { return {frame,DialogueCueKind::BossMusic,0,1}; }
constexpr auto kBeforeCues=std::to_array<DialogueCue>({
    P(0,0,1),P(1434,1,1),P(1434,0,4),
    P(1744,0,3),P(1744,1,4),I(1745,false),I(1775,true),
    P(2045,0,4),P(2045,1,3),P(2346,0,3),P(2346,1,4),
    P(2648,0,4),P(2648,1,3),P(2949,0,3),P(2949,1,4),
    P(3250,0,4),P(3250,1,3),P(3551,0,3),P(3551,1,4),
    P(3852,0,4),P(3852,1,3),M(3852),P(4153,0,3),P(4153,1,4),
    P(4653,0,5),P(4653,1,5),I(4653,false),
});
constexpr auto kAfterCues=std::to_array<DialogueCue>({P(0,0,1),I(1260,false)});
constexpr auto kMarisaBeforeCues=std::to_array<DialogueCue>({
    P(0,0,1),P(1363,1,1),P(1363,0,4),P(1564,0,3),P(1564,1,4),
    I(1564,false),I(1594,true),P(1865,0,4),P(1865,1,3),
    P(2166,0,3),P(2166,1,4),P(2467,0,4),P(2467,1,3),M(2467),
    P(2768,0,3),P(2768,1,4),P(3168,0,5),P(3168,1,5),I(3168,false),
});
constexpr auto kMarisaAfterCues=std::to_array<DialogueCue>({P(0,0,1),I(860,false)});
}
std::span<const DialogueLine> Stage1Dialogue(bool afterBattle, bool marisa) {
    if(marisa)return afterBattle ? std::span<const DialogueLine>(kMarisaAfter) : std::span<const DialogueLine>(kMarisaBefore);
    return afterBattle ? std::span<const DialogueLine>(kAfter) : std::span<const DialogueLine>(kBefore);
}
std::span<const DialogueCue> Stage1DialogueCues(bool afterBattle,bool marisa) {
    if(marisa)return afterBattle ? std::span<const DialogueCue>(kMarisaAfterCues) : std::span<const DialogueCue>(kMarisaBeforeCues);
    return afterBattle ? std::span<const DialogueCue>(kAfterCues) : std::span<const DialogueCue>(kBeforeCues);
}
} // namespace koumakyou::stage1
