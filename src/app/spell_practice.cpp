// 本文件保存原典符卡身份与本地化目录，并按实际 Boss 使用者组织跨关卡浏览层级。
#include "app/spell_practice.hpp"
#include "stage/catalog.hpp"
namespace koumakyou::spell {
namespace {
constexpr std::array<Stage,7> kStages{{
    {"STAGE 1","梦幻夜行绘卷　～ Mystic Flier","露米娅",3,9},
    {"STAGE 2","湖面的魔精　～ Water Magus","琪露诺",3,11},
    {"STAGE 3","红色境域　～ Scarlet Land","红美铃",5,17},
    {"STAGE 4","昏暗宅邸　～ Save the mind.","帕秋莉·诺蕾姬",16,47},
    {"STAGE 5","潇洒从者寄身红月","十六夜咲夜",4,16},
    {"STAGE 6","至福乐土降下血雨","蕾米莉亚·斯卡蕾特",6,18},
    {"STAGE EX","东方红魔狂　～ Sister of Scarlet","芙兰朵露·斯卡蕾特",13,13},
}};
// 浏览组仅引用原始身份：六面道中属于咲夜，Extra 前三符属于帕秋莉。
constexpr Group kRumiaGroups[]{{0,0},{0,1},{0,2}};
constexpr Group kCirnoGroups[]{{1,0},{1,1},{1,2}};
constexpr Group kMeilingGroups[]{{2,0},{2,1},{2,2},{2,3},{2,4}};
constexpr Group kPatchouliGroups[]{
    {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},
    {3,8},{3,9},{3,10},{3,11},{3,12},{3,13},{3,14},{3,15},{6,0},{6,1},{6,2}};
constexpr Group kSakuyaGroups[]{{4,0},{4,1},{4,2},{4,3},{5,0}};
constexpr Group kRemiliaGroups[]{{5,1},{5,2},{5,3},{5,4},{5,5}};
constexpr Group kFlandreGroups[]{{6,3},{6,4},{6,5},{6,6},{6,7},{6,8},{6,9},{6,10},{6,11},{6,12}};
constexpr std::array<Boss,kBossCount> kBosses{{
    {"露米娅",kRumiaGroups},{"琪露诺",kCirnoGroups},{"红美铃",kMeilingGroups},
    {"帕秋莉·诺蕾姬",kPatchouliGroups},{"十六夜咲夜",kSakuyaGroups},
    {"蕾米莉亚·斯卡蕾特",kRemiliaGroups},{"芙兰朵露·斯卡蕾特",kFlandreGroups},
}};
constexpr std::array<Character,4> kCharacters{{
    {"博丽灵梦（灵）","博丽神社的巫女小姐","博丽 灵梦","灵之御札「Homing Amulet」","「梦想封印」",0,{3,4,3}},
    {"博丽灵梦（梦）","博丽神社的巫女小姐","博丽 灵梦","梦之御札「Persuasion Needle」","「封魔阵」",0,{3,4,3}},
    {"雾雨魔理沙（魔）","东洋的西洋魔术师","雾雨 魔理沙","魔之御札「Magic Missile」","「Stardust Reverie」",1,{4,1,4}},
    {"雾雨魔理沙（恋）","东洋的西洋魔术师","雾雨 魔理沙","恋之御札「Illusion Laser」","「Master Spark」",1,{4,1,4}},
}};
constexpr std::array<Card,kSpellCount> kCards{{
    {1,0,0,2,"月符「Moonlight Ray」","露米娅",2,32},
    {2,0,0,3,"月符「Moonlight Ray」","露米娅",2,32},
    {3,0,1,1,"夜符「Night Bird」","露米娅",2,64},
    {4,0,1,2,"夜符「Night Bird」","露米娅",2,64},
    {5,0,1,3,"夜符「Night Bird」","露米娅",2,64},
    {6,0,2,0,"暗符「Demarcation」","露米娅",3,64},
    {7,0,2,1,"暗符「Demarcation」","露米娅",4,64},
    {8,0,2,2,"暗符「Demarcation」","露米娅",4,64},
    {9,0,2,3,"暗符「Demarcation」","露米娅",4,64},
    {10,1,0,0,"冰符「Icicle Fall」","琪露诺",2},
    {11,1,0,1,"冰符「Icicle Fall」","琪露诺",2},
    {12,1,0,2,"雹符「Hailstorm」","琪露诺",2},
    {13,1,0,3,"雹符「Hailstorm」","琪露诺",2},
    {14,1,1,0,"冻符「Perfect Freeze」","琪露诺",4},
    {15,1,1,1,"冻符「Perfect Freeze」","琪露诺",4},
    {16,1,1,2,"冻符「Perfect Freeze」","琪露诺",4},
    {17,1,1,3,"冻符「Perfect Freeze」","琪露诺",4},
    {18,1,2,1,"雪符「Diamond Blizzard」","琪露诺",5},
    {19,1,2,2,"雪符「Diamond Blizzard」","琪露诺",5},
    {20,1,2,3,"雪符「Diamond Blizzard」","琪露诺",5},
    {21,2,0,0,"华符「芳华绚烂」","红美铃",2},
    {22,2,0,1,"华符「芳华绚烂」","红美铃",2},
    {23,2,0,2,"华符「Selaginella 9」","红美铃",2},
    {24,2,0,3,"华符「Selaginella 9」","红美铃",2},
    {25,2,1,0,"虹符「彩虹的风铃」","红美铃",2},
    {26,2,1,1,"虹符「彩虹的风铃」","红美铃",2},
    {27,2,1,2,"虹符「彩虹的风铃」","红美铃",2},
    {28,2,1,3,"虹符「彩虹的风铃」","红美铃",2},
    {29,2,2,2,"幻符「华想梦葛」","红美铃",4},
    {30,2,2,3,"幻符「华想梦葛」","红美铃",4},
    {31,2,3,0,"彩符「彩雨」","红美铃",5},
    {32,2,3,1,"彩符「彩雨」","红美铃",5},
    {33,2,3,2,"彩符「彩光乱舞」","红美铃",6},
    {34,2,3,3,"彩符「彩光乱舞」","红美铃",6},
    {35,2,4,1,"彩符「极彩台风」","红美铃",6},
    {36,2,4,2,"彩符「极彩台风」","红美铃",7},
    {37,2,4,3,"彩符「极彩台风」","红美铃",7},
    {38,3,0,0,"火符「Agni Shine」","帕秋莉·诺蕾姬",1},
    {39,3,0,1,"火符「Agni Shine」","帕秋莉·诺蕾姬",1},
    {40,3,1,1,"火符「Agni Shine上级」","帕秋莉·诺蕾姬",1},
    {41,3,1,2,"火符「Agni Shine上级」","帕秋莉·诺蕾姬",1},
    {42,3,1,3,"火符「Agni Shine上级」","帕秋莉·诺蕾姬",1},
    {43,3,2,2,"火符「Agni Radiance」","帕秋莉·诺蕾姬",1},
    {44,3,2,3,"火符「Agni Radiance」","帕秋莉·诺蕾姬",1},
    {45,3,3,0,"水符「Princess Undine」","帕秋莉·诺蕾姬",1},
    {46,3,3,1,"水符「Princess Undine」","帕秋莉·诺蕾姬",1},
    {47,3,3,2,"水符「Bury in Lake」","帕秋莉·诺蕾姬",1},
    {48,3,3,3,"水符「Bury in Lake」","帕秋莉·诺蕾姬",1},
    {49,3,4,0,"木符「Sylphy Horn」","帕秋莉·诺蕾姬",1},
    {50,3,4,1,"木符「Sylphy Horn」","帕秋莉·诺蕾姬",1},
    {51,3,5,1,"木符「Sylphy Horn上级」","帕秋莉·诺蕾姬",1},
    {52,3,5,2,"木符「Sylphy Horn上级」","帕秋莉·诺蕾姬",1},
    {53,3,5,3,"木符「Sylphy Horn上级」","帕秋莉·诺蕾姬",1},
    {54,3,6,2,"木符「Green Storm」","帕秋莉·诺蕾姬",1},
    {55,3,6,3,"木符「Green Storm」","帕秋莉·诺蕾姬",1},
    {56,3,7,0,"土符「Lazy Trilithon」","帕秋莉·诺蕾姬",1},
    {57,3,7,1,"土符「Lazy Trilithon」","帕秋莉·诺蕾姬",1},
    {58,3,8,1,"土符「Lazy Trilithon上级」","帕秋莉·诺蕾姬",1},
    {59,3,8,2,"土符「Lazy Trilithon上级」","帕秋莉·诺蕾姬",1},
    {60,3,8,3,"土符「Lazy Trilithon上级」","帕秋莉·诺蕾姬",1},
    {61,3,9,2,"土符「Trilithon Shake」","帕秋莉·诺蕾姬",1},
    {62,3,9,3,"土符「Trilithon Shake」","帕秋莉·诺蕾姬",1},
    {63,3,10,1,"金符「Metal Fatigue」","帕秋莉·诺蕾姬",1},
    {64,3,10,2,"金符「Silver Dragon」","帕秋莉·诺蕾姬",1},
    {65,3,10,3,"金符「Silver Dragon」","帕秋莉·诺蕾姬",1},
    {66,3,11,0,"火&土符「Lava Cromlech」","帕秋莉·诺蕾姬",1},
    {67,3,11,1,"火&土符「Lava Cromlech」","帕秋莉·诺蕾姬",1},
    {68,3,11,2,"火&土符「Lava Cromlech」","帕秋莉·诺蕾姬",1},
    {69,3,11,3,"火&土符「Lava Cromlech」","帕秋莉·诺蕾姬",1},
    {70,3,12,0,"木&火符「Forest Blaze」","帕秋莉·诺蕾姬",1},
    {71,3,12,1,"木&火符「Forest Blaze」","帕秋莉·诺蕾姬",1},
    {72,3,12,2,"木&火符「Forest Blaze」","帕秋莉·诺蕾姬",1},
    {73,3,12,3,"木&火符「Forest Blaze」","帕秋莉·诺蕾姬",1},
    {74,3,13,0,"水&木符「Water Elf」","帕秋莉·诺蕾姬",1},
    {75,3,13,1,"水&木符「Water Elf」","帕秋莉·诺蕾姬",1},
    {76,3,13,2,"水&木符「Water Elf」","帕秋莉·诺蕾姬",1},
    {77,3,13,3,"水&木符「Water Elf」","帕秋莉·诺蕾姬",1},
    {78,3,14,1,"金&水符「Mercury Poison」","帕秋莉·诺蕾姬",1},
    {79,3,14,2,"金&水符「Mercury Poison」","帕秋莉·诺蕾姬",1},
    {80,3,14,3,"金&水符「Mercury Poison」","帕秋莉·诺蕾姬",1},
    {81,3,15,0,"土&金符「Emerald Megalith」","帕秋莉·诺蕾姬",1},
    {82,3,15,1,"土&金符「Emerald Megalith」","帕秋莉·诺蕾姬",1},
    {83,3,15,2,"土&金符「Emerald Megalith」","帕秋莉·诺蕾姬",1},
    {84,3,15,3,"土&金符「Emerald Megalith」","帕秋莉·诺蕾姬",1},
    {85,4,0,0,"奇术「Misdirection」","十六夜咲夜",1},
    {86,4,0,1,"奇术「Misdirection」","十六夜咲夜",1},
    {87,4,0,2,"奇术「幻惑Misdirection」","十六夜咲夜",1},
    {88,4,0,3,"奇术「幻惑Misdirection」","十六夜咲夜",1},
    {89,4,1,0,"幻在「Clock Corpse」","十六夜咲夜",1},
    {90,4,1,1,"幻在「Clock Corpse」","十六夜咲夜",1},
    {91,4,1,2,"幻幽「Jack the Ludo Bile」","十六夜咲夜",1},
    {92,4,1,3,"幻幽「Jack the Ludo Bile」","十六夜咲夜",1},
    {93,4,2,0,"幻象「Luna Clock」","十六夜咲夜",1},
    {94,4,2,1,"幻象「Luna Clock」","十六夜咲夜",1},
    {95,4,2,2,"幻世「The World」","十六夜咲夜",1},
    {96,4,2,3,"幻世「The World」","十六夜咲夜",1},
    {97,4,3,0,"女仆秘技「提线玩偶」","十六夜咲夜",1},
    {98,4,3,1,"女仆秘技「提线玩偶」","十六夜咲夜",1},
    {99,4,3,2,"女仆秘技「杀人玩偶」","十六夜咲夜",1},
    {100,4,3,3,"女仆秘技「杀人玩偶」","十六夜咲夜",1},
    {101,5,0,1,"奇术「Eternal Meek」","十六夜咲夜",1},
    {102,5,0,2,"奇术「Eternal Meek」","十六夜咲夜",1},
    {103,5,0,3,"奇术「Eternal Meek」","十六夜咲夜",1},
    {104,5,1,1,"天罚「Star of Vanities」","蕾米莉亚·斯卡蕾特",1},
    {105,5,1,2,"神罚「幼小的恶魔领主」","蕾米莉亚·斯卡蕾特",1},
    {106,5,1,3,"神罚「幼小的恶魔领主」","蕾米莉亚·斯卡蕾特",1},
    {107,5,2,1,"冥符「红色冥界」","蕾米莉亚·斯卡蕾特",1},
    {108,5,2,2,"狱符「千针山」","蕾米莉亚·斯卡蕾特",1},
    {109,5,2,3,"狱符「千针山」","蕾米莉亚·斯卡蕾特",1},
    {110,5,3,1,"咒诅「弗拉德·采佩什的诅咒」","蕾米莉亚·斯卡蕾特",1},
    {111,5,3,2,"神术「吸血鬼幻想」","蕾米莉亚·斯卡蕾特",1},
    {112,5,3,3,"神术「吸血鬼幻想」","蕾米莉亚·斯卡蕾特",1},
    {113,5,4,1,"红符「Scarlet Shoot」","蕾米莉亚·斯卡蕾特",1},
    {114,5,4,2,"红符「Scarlet Meister」","蕾米莉亚·斯卡蕾特",1},
    {115,5,4,3,"红符「Scarlet Meister」","蕾米莉亚·斯卡蕾特",1},
    {116,5,5,1,"「Red Magic」","蕾米莉亚·斯卡蕾特",1},
    {117,5,5,2,"「红色幻想乡」","蕾米莉亚·斯卡蕾特",1},
    {118,5,5,3,"「红色幻想乡」","蕾米莉亚·斯卡蕾特",1},
    {119,6,0,4,"月符「Silent Selene」","帕秋莉·诺蕾姬",1},
    {120,6,1,4,"日符「Royal Flare」","帕秋莉·诺蕾姬",1},
    {121,6,2,4,"火水木金土符「贤者之石」","帕秋莉·诺蕾姬",1},
    {122,6,3,4,"禁忌「Cranberry Trap」","芙兰朵露·斯卡蕾特",1},
    {123,6,4,4,"禁忌「Laevatein」","芙兰朵露·斯卡蕾特",1},
    {124,6,5,4,"禁忌「Four of a Kind」","芙兰朵露·斯卡蕾特",1},
    {125,6,6,4,"禁忌「笼中鸟」","芙兰朵露·斯卡蕾特",1},
    {126,6,7,4,"禁忌「恋之迷宫」","芙兰朵露·斯卡蕾特",1},
    {127,6,8,4,"禁弹「Starbow Break」","芙兰朵露·斯卡蕾特",1},
    {128,6,9,4,"禁弹「Catadioptric」","芙兰朵露·斯卡蕾特",1},
    {129,6,10,4,"禁弹「铭刻往昔之钟」","芙兰朵露·斯卡蕾特",1},
    {130,6,11,4,"秘弹「无人生还？」","芙兰朵露·斯卡蕾特",1},
    {131,6,12,4,"QED「495年的波纹」","芙兰朵露·斯卡蕾特",1},
}};
}
std::span<const Stage> Stages(){return kStages;}
std::span<const Boss> Bosses(){return kBosses;}
int BossIndex(int stage,int group) {
    for(int boss=0;boss<kBossCount;++boss)
        for(const auto& entry:kBosses[boss].groups)
            if(entry.stage==stage&&entry.group==group)return boss;
    return -1;
}
int BossGroupIndex(int stage,int group) {
    const int boss=BossIndex(stage,group);
    if(boss<0)return -1;
    const auto groups=kBosses[boss].groups;
    for(int index=0;index<static_cast<int>(groups.size());++index)
        if(groups[index].stage==stage&&groups[index].group==group)return index;
    return -1;
}
std::span<const Character> Characters(){return kCharacters;}
std::span<const Card> Cards(){return kCards;}
const Card* Find(int stage,int group,int difficulty) {
    for(const auto& card:kCards)if(card.stage==stage && card.group==group && card.difficulty==difficulty)return &card;
    return nullptr;
}
bool Playable(const Card* card,int character) {
    if(!card||card->phase<=0||character<0||character>=kCharacterCount)return false;
    const auto stage=SpellPracticeStage(card->stage);
    // Extra的UI难度槽4只映射其单套Normal脚本，不得开放主线难度槽或把4传入Difficulty枚举。
    if(stage==StageId::Extra&&card->difficulty!=4)return false;
    const auto difficulty=stage==StageId::Extra?std::optional{native_combat::Difficulty::Normal}:
        native_combat::DifficultyFromIndex(card->difficulty);
    return difficulty&&stage&&SupportsStage(*stage,*difficulty,StageEntry::SpellPractice);
}
const char* GroupUser(int stage,int group) {
    for(const auto& card:kCards)if(card.stage==stage && card.group==group)return card.user;
    return "";
}
} // namespace koumakyou::spell
