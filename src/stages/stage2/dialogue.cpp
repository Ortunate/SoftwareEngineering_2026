// 本文件保存新典二面两位自机的中文原文、表情及实际 60 Hz 对话帧；等待与输入锁均由 MSG 展开。
#include "stages/stage2/data.hpp"
#include <array>
namespace koumakyou::stage2 {
namespace {
constexpr std::array<DialogueLine,9> kReimuBefore{{
    {"灵梦","这湖原本有这么大吗？",false,361.0F/60,0,false,false,"",5,60.0F/60,0,68.0F/60,-1},
    {"灵梦","雾蒙蒙的根本看不清远处啊。",false,361.0F/60,0,false,false,"",0,0.0F/60,0,8.0F/60,-1},
    {"灵梦","难不成我是路痴？",false,330.0F/60,0,false,false,"",5,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","迷失方向，是妖精在作怪哦",true,301.0F/60,0,true,true,"",0,0.0F/60,0,30.0F/60,-1},
    {"灵梦","是吗？那，你来带个路？",false,301.0F/60,0,true,false,"这附近应该有座岛吧？",2,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","你倒是惊讶一点好不好？",true,301.0F/60,0,true,false,"一个强敌正拦在你面前呢",1,0.0F/60,0,8.0F/60,-1},
    {"灵梦","枪靶？",false,301.0F/60,0,true,false,"这可真是令人吃惊",3,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","竟敢笑话咱~",true,301.0F/60,0,true,false,"",1,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","看咱不把你冷冻保存起来",true,430.0F/60,0,true,false,"一直冻到忘干净为止！！",1,0.0F/60,0,8.0F/60,400.0F/60},
}};
constexpr std::array<DialogueCue,19> kReimuBeforeCues{{
    {0,DialogueCueKind::Portrait,0,1},
    {1052,DialogueCueKind::Portrait,1,1},
    {1052,DialogueCueKind::Portrait,0,4},
    {1052,DialogueCueKind::BossMusic,0,1},
    {1052,DialogueCueKind::InputEnabled,0,0},
    {1082,DialogueCueKind::InputEnabled,0,1},
    {1352,DialogueCueKind::Portrait,0,3},
    {1352,DialogueCueKind::Portrait,1,4},
    {1653,DialogueCueKind::Portrait,0,4},
    {1653,DialogueCueKind::Portrait,1,3},
    {1954,DialogueCueKind::Portrait,0,3},
    {1954,DialogueCueKind::Portrait,1,4},
    {2255,DialogueCueKind::Portrait,0,4},
    {2255,DialogueCueKind::Portrait,1,3},
    {2556,DialogueCueKind::Portrait,0,4},
    {2556,DialogueCueKind::Portrait,1,3},
    {2957,DialogueCueKind::Portrait,0,5},
    {2957,DialogueCueKind::Portrait,1,5},
    {2957,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kReimuAfter{{
    {"灵梦","哎呀，越来越冷了",false,360.0F/60,0,false,false,"",5,60.0F/60,0,68.0F/60,-1},
    {"灵梦","这样会得空调病的",false,960.0F/60,0,false,false,"",5,0.0F/60,0,8.0F/60,900.0F/60},
}};
constexpr std::array<DialogueCue,2> kReimuAfterCues{{
    {0,DialogueCueKind::Portrait,0,1},
    {1260,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,10> kMarisaBefore{{
    {"魔理沙","我记得小岛是在",false,361.0F/60,0,false,false,"这边来着吧......",0,60.0F/60,0,68.0F/60,-1},
    {"魔理沙","难不成还长腿跑了？",false,301.0F/60,0,false,false,"",5,0.0F/60,0,8.0F/60,-1},
    {"魔理沙","话说回来......",false,301.0F/60,0,false,false,"",5,0.0F/60,0,8.0F/60,-1},
    {"魔理沙","差不多都夏天了吧",false,330.0F/60,0,false,false,"为啥还这么冷啊？",0,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","咱要让你再也上不了岸！",true,301.0F/60,0,true,true,"",0,0.0F/60,0,30.0F/60,-1},
    {"魔理沙","这么冷是你在捣鬼吧。",false,301.0F/60,0,true,false,"",3,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","总比大热天要好吧？",true,301.0F/60,0,true,false,"",1,0.0F/60,0,8.0F/60,-1},
    {"魔理沙","好一个冷场高手",false,301.0F/60,0,true,false,"",3,0.0F/60,0,8.0F/60,-1},
    {"琪露诺","这说法怎么感觉不太对...",true,301.0F/60,0,true,false,"",1,0.0F/60,0,8.0F/60,-1},
    {"魔理沙","吃不消了吧？",false,430.0F/60,0,true,false,"",3,0.0F/60,0,8.0F/60,400.0F/60},
}};
constexpr std::array<DialogueCue,19> kMarisaBeforeCues{{
    {0,DialogueCueKind::Portrait,0,1},
    {1293,DialogueCueKind::Portrait,1,1},
    {1293,DialogueCueKind::Portrait,0,4},
    {1293,DialogueCueKind::BossMusic,0,1},
    {1293,DialogueCueKind::InputEnabled,0,0},
    {1323,DialogueCueKind::InputEnabled,0,1},
    {1594,DialogueCueKind::Portrait,0,3},
    {1594,DialogueCueKind::Portrait,1,4},
    {1894,DialogueCueKind::Portrait,0,4},
    {1894,DialogueCueKind::Portrait,1,3},
    {2195,DialogueCueKind::Portrait,0,3},
    {2195,DialogueCueKind::Portrait,1,4},
    {2496,DialogueCueKind::Portrait,0,4},
    {2496,DialogueCueKind::Portrait,1,3},
    {2797,DialogueCueKind::Portrait,0,3},
    {2797,DialogueCueKind::Portrait,1,4},
    {3198,DialogueCueKind::Portrait,0,5},
    {3198,DialogueCueKind::Portrait,1,5},
    {3198,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kMarisaAfter{{
    {"魔理沙","唉，只穿短袖容易着凉啊",false,361.0F/60,0,false,false,"",1,60.0F/60,0,68.0F/60,-1},
    {"魔理沙","走喽",false,560.0F/60,0,false,false,"赶紧找户人家讨茶喝",3,0.0F/60,0,8.0F/60,500.0F/60},
}};
constexpr std::array<DialogueCue,2> kMarisaAfterCues{{
    {0,DialogueCueKind::Portrait,0,1},
    {861,DialogueCueKind::InputEnabled,0,0},
}};
}
std::span<const DialogueLine> Stage2Dialogue(bool afterBattle,bool marisa) {
    if(marisa)return afterBattle ? std::span<const DialogueLine>(kMarisaAfter) : std::span<const DialogueLine>(kMarisaBefore);
    return afterBattle ? std::span<const DialogueLine>(kReimuAfter) : std::span<const DialogueLine>(kReimuBefore);
}
std::span<const DialogueCue> Stage2DialogueCues(bool afterBattle,bool marisa) {
    if(marisa)return afterBattle ? std::span<const DialogueCue>(kMarisaAfterCues) : std::span<const DialogueCue>(kMarisaBeforeCues);
    return afterBattle ? std::span<const DialogueCue>(kReimuAfterCues) : std::span<const DialogueCue>(kReimuBeforeCues);
}
}
