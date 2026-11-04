// 本文件保存新典三面两位自机的中文原文、表情及实际 60 Hz 对话帧；等待与输入锁均由 MSG 展开。
#include "stages/stage3/data.hpp"
#include <array>
namespace koumakyou::stage3 {
namespace {
constexpr std::array<DialogueLine,14> kReimuBefore{{
    {"红美铃","你别跟着我呀~",true,361.0F/60,60.0F/60,true,false,"",0,60.0F/60,0,68.0F/60,-1,false,0.0F/60},
    {"灵梦","多谢带路~",false,301.0F/60,0.0F/60,true,false,"",0,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","唉呀，这边就是一片空地",true,361.0F/60,0.0F/60,true,true,"你跟着我又能怎样呢？",0,0.0F/60,0,30.0F/60,-1,false,0.0F/60},
    {"灵梦","哪有人会朝着空地逃跑的？",false,361.0F/60,0.0F/60,true,false,"",3,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","嗯——真要逃跑的时候",true,362.0F/60,1.0F/60,true,false,"可管不了这么多吧",0,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"灵梦","顺便问一句，你是什么人？",false,361.0F/60,0.0F/60,true,false,"",5,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","哎——我就是个普通人",true,361.0F/60,0.0F/60,true,false,"",1,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"灵梦","可刚才你出手攻击我了吧？",false,361.0F/60,0.0F/60,true,false,"",4,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","那是普通的攻击",true,361.0F/60,0.0F/60,true,false,"再说了，是你先动手的好吧",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","你这人，可不普通啊",true,361.0F/60,0.0F/60,true,false,"",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"灵梦","我是个当巫女的普通人哦",false,361.0F/60,0.0F/60,true,false,"",0,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","那可太好了",true,362.0F/60,1.0F/60,true,false,"我记得...",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","传说巫女是一种",true,362.0F/60,1.0F/60,true,false,"可以吃的人类...",0,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"灵梦","别传这种说！",false,390.0F/60,0.0F/60,true,false,"",5,0.0F/60,0,8.0F/60,360.0F/60,false,0.0F/60},
}};
constexpr std::array<DialogueCue,47> kReimuBeforeCues{{
    {0,DialogueCueKind::Expression,1,0},
    {0,DialogueCueKind::Portrait,1,1},
    {0,DialogueCueKind::InputEnabled,0,0},
    {60,DialogueCueKind::InputEnabled,0,1},
    {361,DialogueCueKind::Expression,0,0},
    {361,DialogueCueKind::Portrait,0,1},
    {361,DialogueCueKind::Portrait,1,4},
    {661,DialogueCueKind::Portrait,0,4},
    {661,DialogueCueKind::Portrait,1,3},
    {662,DialogueCueKind::Expression,1,0},
    {1022,DialogueCueKind::Portrait,0,3},
    {1022,DialogueCueKind::Portrait,1,4},
    {1023,DialogueCueKind::Expression,0,3},
    {1383,DialogueCueKind::Portrait,0,4},
    {1383,DialogueCueKind::Portrait,1,3},
    {1385,DialogueCueKind::Expression,1,1},
    {1745,DialogueCueKind::Portrait,0,3},
    {1745,DialogueCueKind::Portrait,1,4},
    {1746,DialogueCueKind::Expression,0,5},
    {2106,DialogueCueKind::Portrait,0,4},
    {2106,DialogueCueKind::Portrait,1,3},
    {2107,DialogueCueKind::Expression,1,1},
    {2467,DialogueCueKind::Portrait,0,3},
    {2467,DialogueCueKind::Portrait,1,4},
    {2468,DialogueCueKind::Expression,0,4},
    {2828,DialogueCueKind::Portrait,0,4},
    {2828,DialogueCueKind::Portrait,1,3},
    {2829,DialogueCueKind::Expression,1,2},
    {3189,DialogueCueKind::Portrait,0,4},
    {3189,DialogueCueKind::Portrait,1,3},
    {3190,DialogueCueKind::Expression,0,5},
    {3550,DialogueCueKind::Portrait,0,3},
    {3550,DialogueCueKind::Portrait,1,4},
    {3551,DialogueCueKind::Expression,0,0},
    {3911,DialogueCueKind::Portrait,0,4},
    {3911,DialogueCueKind::Portrait,1,3},
    {3913,DialogueCueKind::Expression,1,0},
    {4273,DialogueCueKind::Portrait,0,4},
    {4273,DialogueCueKind::Portrait,1,3},
    {4275,DialogueCueKind::Expression,1,0},
    {4275,DialogueCueKind::BossMusic,0,1},
    {4635,DialogueCueKind::Portrait,0,3},
    {4635,DialogueCueKind::Portrait,1,4},
    {4636,DialogueCueKind::Expression,0,5},
    {4996,DialogueCueKind::Portrait,0,5},
    {4996,DialogueCueKind::Portrait,1,5},
    {4996,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kReimuAfter{{
    {"灵梦","好，那就请你带路吧",false,422.0F/60,61.0F/60,true,false,"",2,61.0F/60,0,69.0F/60,-1,false,0.0F/60},
    {"红美铃","对不起，大小姐~",true,960.0F/60,0.0F/60,true,false,"",3,0.0F/60,0,8.0F/60,900.0F/60,false,0.0F/60},
}};
constexpr std::array<DialogueCue,9> kReimuAfterCues{{
    {0,DialogueCueKind::Expression,0,2},
    {0,DialogueCueKind::Portrait,0,1},
    {0,DialogueCueKind::Expression,1,3},
    {0,DialogueCueKind::Portrait,1,1},
    {0,DialogueCueKind::Portrait,0,3},
    {0,DialogueCueKind::Portrait,1,4},
    {421,DialogueCueKind::Portrait,0,4},
    {421,DialogueCueKind::Portrait,1,3},
    {1322,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kReimuMidAfter{{
    {"红美铃","可恶，得摆背水阵了！",true,180.0F/60,60.0F/60,true,false,"",0,60.0F/60,0,68.0F/60,-1,false,0.0F/60},
    {"灵梦","你一个人也算「阵」吗？",false,90.0F/60,0.0F/60,true,false,"",1,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
}};
constexpr std::array<DialogueCue,7> kReimuMidAfterCues{{
    {0,DialogueCueKind::InputEnabled,0,0},
    {0,DialogueCueKind::Expression,1,0},
    {0,DialogueCueKind::Portrait,1,1},
    {120,DialogueCueKind::Expression,0,1},
    {120,DialogueCueKind::Portrait,0,1},
    {180,DialogueCueKind::Portrait,1,5},
    {240,DialogueCueKind::Portrait,0,5},
}};
constexpr std::array<DialogueLine,11> kMarisaBefore{{
    {"红美铃","啊，刚才真是谢谢了",true,361.0F/60,60.0F/60,true,false,"",0,60.0F/60,0,68.0F/60,-1,false,0.0F/60},
    {"魔理沙","好久不见啊",false,301.0F/60,0.0F/60,true,false,"",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","不对，我跟你什么时候",true,362.0F/60,1.0F/60,true,true,"混成熟人了~",0,0.0F/60,0,30.0F/60,-1,false,1.0F/60},
    {"魔理沙","就刚才呀？",false,361.0F/60,0.0F/60,true,false,"",3,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","呃——遇到怪人了",true,362.0F/60,1.0F/60,true,false,"",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"魔理沙","好了，别挡路",false,362.0F/60,1.0F/60,true,false,"你是这里的守门人吧？",3,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","正因为是守门人，所以才要挡路",true,361.0F/60,0.0F/60,true,false,"",2,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"魔理沙","你果真是守门人？",false,361.0F/60,0.0F/60,true,false,"",4,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","我只是个负责守门的普通人",true,361.0F/60,0.0F/60,true,false,"",0,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"魔理沙","说到底就是普通人呗",false,362.0F/60,1.0F/60,true,false,"那我要将你就地处决",4,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
    {"红美铃","你这家伙，受的是哪门子教育啊",true,390.0F/60,0.0F/60,true,false,"",1,0.0F/60,0,8.0F/60,360.0F/60,false,0.0F/60},
}};
constexpr std::array<DialogueCue,38> kMarisaBeforeCues{{
    {0,DialogueCueKind::Expression,1,0},
    {0,DialogueCueKind::Portrait,1,1},
    {0,DialogueCueKind::InputEnabled,0,0},
    {60,DialogueCueKind::InputEnabled,0,1},
    {361,DialogueCueKind::Expression,0,2},
    {361,DialogueCueKind::Portrait,0,1},
    {361,DialogueCueKind::Portrait,1,4},
    {661,DialogueCueKind::Portrait,0,4},
    {661,DialogueCueKind::Portrait,1,3},
    {663,DialogueCueKind::Expression,1,2},
    {1023,DialogueCueKind::Portrait,0,3},
    {1023,DialogueCueKind::Portrait,1,4},
    {1024,DialogueCueKind::Expression,0,3},
    {1384,DialogueCueKind::Portrait,0,4},
    {1384,DialogueCueKind::Portrait,1,3},
    {1386,DialogueCueKind::Expression,1,1},
    {1746,DialogueCueKind::Portrait,0,3},
    {1746,DialogueCueKind::Portrait,1,4},
    {1748,DialogueCueKind::Expression,0,0},
    {2108,DialogueCueKind::Portrait,0,4},
    {2108,DialogueCueKind::Portrait,1,3},
    {2109,DialogueCueKind::Expression,1,2},
    {2469,DialogueCueKind::Portrait,0,3},
    {2469,DialogueCueKind::Portrait,1,4},
    {2470,DialogueCueKind::Expression,0,4},
    {2830,DialogueCueKind::Portrait,0,4},
    {2830,DialogueCueKind::Portrait,1,3},
    {2831,DialogueCueKind::Expression,1,0},
    {3191,DialogueCueKind::Portrait,0,3},
    {3191,DialogueCueKind::Portrait,1,4},
    {3193,DialogueCueKind::Expression,0,1},
    {3193,DialogueCueKind::BossMusic,0,1},
    {3553,DialogueCueKind::Portrait,0,4},
    {3553,DialogueCueKind::Portrait,1,3},
    {3554,DialogueCueKind::Expression,1,1},
    {3914,DialogueCueKind::Portrait,0,5},
    {3914,DialogueCueKind::Portrait,1,5},
    {3914,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kMarisaAfter{{
    {"魔理沙","跟普通人战斗",false,422.0F/60,61.0F/60,true,false,"到底不合我的性子啊",3,61.0F/60,0,69.0F/60,-1,false,0.0F/60},
    {"红美铃","胡扯~",true,960.0F/60,0.0F/60,true,false,"",3,0.0F/60,0,8.0F/60,900.0F/60,false,0.0F/60},
}};
constexpr std::array<DialogueCue,9> kMarisaAfterCues{{
    {0,DialogueCueKind::Expression,0,3},
    {0,DialogueCueKind::Portrait,0,1},
    {0,DialogueCueKind::Expression,1,3},
    {0,DialogueCueKind::Portrait,1,1},
    {0,DialogueCueKind::Portrait,0,3},
    {0,DialogueCueKind::Portrait,1,4},
    {421,DialogueCueKind::Portrait,0,4},
    {421,DialogueCueKind::Portrait,1,3},
    {1322,DialogueCueKind::InputEnabled,0,0},
}};
constexpr std::array<DialogueLine,2> kMarisaMidAfter{{
    {"红美铃","可恶，先跑再说！",true,180.0F/60,60.0F/60,true,false,"",0,60.0F/60,0,68.0F/60,-1,false,0.0F/60},
    {"魔理沙","想跑就跑呗",false,90.0F/60,0.0F/60,true,false,"",1,0.0F/60,0,8.0F/60,-1,false,0.0F/60},
}};
constexpr std::array<DialogueCue,7> kMarisaMidAfterCues{{
    {0,DialogueCueKind::InputEnabled,0,0},
    {0,DialogueCueKind::Expression,1,0},
    {0,DialogueCueKind::Portrait,1,1},
    {120,DialogueCueKind::Expression,0,1},
    {120,DialogueCueKind::Portrait,0,1},
    {180,DialogueCueKind::Portrait,1,5},
    {240,DialogueCueKind::Portrait,0,5},
}};
}
std::span<const DialogueLine> Stage3Dialogue(DialogueScene scene,bool afterBattle,bool marisa) {
    if(scene==DialogueScene::Midboss)return !afterBattle ? std::span<const DialogueLine>{} : marisa ? std::span<const DialogueLine>(kMarisaMidAfter) : std::span<const DialogueLine>(kReimuMidAfter);
    if(marisa)return afterBattle ? std::span<const DialogueLine>(kMarisaAfter) : std::span<const DialogueLine>(kMarisaBefore);
    return afterBattle ? std::span<const DialogueLine>(kReimuAfter) : std::span<const DialogueLine>(kReimuBefore);
}
std::span<const DialogueCue> Stage3DialogueCues(DialogueScene scene,bool afterBattle,bool marisa) {
    if(scene==DialogueScene::Midboss)return !afterBattle ? std::span<const DialogueCue>{} : marisa ? std::span<const DialogueCue>(kMarisaMidAfterCues) : std::span<const DialogueCue>(kReimuMidAfterCues);
    if(marisa)return afterBattle ? std::span<const DialogueCue>(kMarisaAfterCues) : std::span<const DialogueCue>(kMarisaBeforeCues);
    return afterBattle ? std::span<const DialogueCue>(kReimuAfterCues) : std::span<const DialogueCue>(kReimuBeforeCues);
}
}
