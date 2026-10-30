// 本文件实现全素材目录、弹幕/道具原编号映射以及独立动画游标同步。
#include "resources/battle_catalog.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace koumakyou::battle {
namespace {
// DDS 使用原包的双倍分辨率 _4k 原件，沿用运行时文件名；ANM 逻辑坐标与世界尺寸保持不变。
#define ENTRY(id,name,stage) Definition{Atlas::id,name,"battle/native/" name ".dds","battle/native/" name ".anm",stage}
constexpr std::array<Definition,25> kCatalog{{
    ENTRY(Bullets,"etama3",0),ENTRY(BeamBullets,"etama3_2",0),ENTRY(BulletEffects,"etama4",0),
    ENTRY(Reimu,"player00",0),ENTRY(Marisa,"player01",0),ENTRY(Effects,"eff00",0),
    ENTRY(Stage1Enemies,"stg1enm",1),ENTRY(Rumia,"stg1enm2",1),
    ENTRY(Stage2Enemies,"stg2enm",2),ENTRY(Cirno,"stg2enm2",2),
    ENTRY(Stage3Enemies,"stg3enm",3),ENTRY(Stage4Enemies,"stg4enm",4),
    ENTRY(Stage5Enemies,"stg5enm",5),ENTRY(Sakuya,"stg5enm2",5),
    ENTRY(Stage6Enemies,"stg6enm",6),ENTRY(Remilia,"stg6enm2",6),
    ENTRY(ExtraEnemies,"stg7enm",7),ENTRY(Flandre,"stg7enm2",7),
    ENTRY(Stage1Effects,"eff01",1),ENTRY(Stage2Effects,"eff02",2),ENTRY(Stage3Effects,"eff03",3),
    ENTRY(Stage4Effects,"eff04",4),ENTRY(Stage5Effects,"eff05",5),ENTRY(Stage6Effects,"eff06",6),ENTRY(ExtraEffects,"eff07",7)
}};
#undef ENTRY
constexpr std::array<int,16> kSpawn16{0,1,1,1,1,2,2,2,2,3,3,3,4,4,4,0};
constexpr std::array<int,8> kSpawn8{0,1,1,2,2,3,4,0};
}
std::span<const Definition> Catalog() { return kCatalog; }
const Definition& Describe(Atlas id) {
    const auto index=static_cast<std::size_t>(id);
    if(index>=kCatalog.size())throw std::invalid_argument("Invalid battle atlas");
    return kCatalog[index];
}
Selection EnemyBullet(int type,int color) {
    if(type<0 || type>9 || color<0 || color>15)throw std::invalid_argument("Invalid native bullet type/color");
    const int colors=type<=5 ? 16 : type==7 ? 1 : type==9 ? 5 : 8;
    return {type==9 ? Atlas::BulletEffects : Atlas::Bullets,type==9 ? 0 : type,color%colors};
}
Vector2 EnemyBulletLogicalSize(int type,int color) {
    // 新典加载器为未显式指定逻辑尺寸的精灵应用自身设计分辨率；这里保存换算后的世界结果。
    // 6～9 的逻辑高达到原生“大弹”筛选阈值；1～5 仍属小弹，不能拿裁图的 36/72 像素分类。
    constexpr std::array<Vector2,10> sizes{{{0.357142857F,0.357142857F},{0.714285714F,0.714285714F},
        {0.625F,0.714285714F},{0.714285714F,0.714285714F},{0.625F,0.714285714F},
        {0.625F,0.714285714F},{1.42857143F,1.42857143F},{1.33928571F,1.33928571F},
        {1.42857143F,1.42857143F},{2.85714286F,2.85714286F}}};
    const auto selection=EnemyBullet(type,color);
    // etama4 的最后一色是 30×30，而前四色为 64×64；判定仍由 9 号模板保持直径 32。
    if(type==9&&selection.spriteOffset==4)return {1.33928571F,1.33928571F};
    return sizes[type];
}
float EnemyBulletHitRadius(int type) {
    // EXE RVA 0x12979～0x12a3a 的模板全宽 4/6/4/6/5/4/16/11/9/32，先除二再转为世界单位。
    constexpr std::array<float,10> radii{0.0892857143F,0.133928571F,0.0892857143F,0.133928571F,
        0.111607143F,0.0892857143F,0.357142857F,0.245535714F,0.200892857F,0.714285714F};
    if(type<0||type>=static_cast<int>(radii.size()))throw std::invalid_argument("Invalid native bullet radius");
    return radii[type];
}
Selection BulletSpawn(int type,int color,int frames) {
    const auto body=EnemyBullet(type,color);
    if(frames!=10 && frames!=16 && frames!=32)throw std::invalid_argument("Invalid bullet spawn duration");
    if(type==9)return {Atlas::BulletEffects,2,body.spriteOffset};
    if(type>=6)return {Atlas::Bullets,20,type==7 ? 1 : kSpawn8[body.spriteOffset]};
    return {Atlas::Bullets,(type==0 ? 14 : 17)+(frames==10 ? 0 : frames==16 ? 1 : 2),kSpawn16[color]};
}
Selection Item(int type) {
    if(type<0 || type>6)throw std::invalid_argument("Invalid native item");
    return {Atlas::Bullets,21+type};
}
Selection Player(Atlas character,int action) {
    if((character!=Atlas::Reimu && character!=Atlas::Marisa) || action<0 || action>4)
        throw std::invalid_argument("Invalid player animation");
    return {character,action};
}
void Cursor::Sync(const anm::Archive& archive,Selection selected,double age,std::uint32_t serial,const AnimationInterrupt* interrupt) {
    if(!std::isfinite(age) || age<0)throw std::invalid_argument("Invalid animation age");
    const bool restart=player.Source()!=&archive||selection!=selected||revision!=serial||age+1e-6<time;
    const bool apply=interrupt&&interrupt->revision==serial&&(restart||interrupt->id!=appliedInterrupt);
    if(apply&&(!interrupt->id||!std::isfinite(interrupt->age)||interrupt->age<0||interrupt->age>age+1e-6))
        throw std::invalid_argument("Invalid animation interrupt age");
    if(restart||
       (apply&&interrupt->age+1e-6<time)) {
        player.Start(archive,selected.script,selected.spriteOffset,selected.initialSprite);
        selection=selected;revision=serial;time=0;appliedInterrupt=0;
    }
    if(interrupt&&interrupt->revision==serial&&interrupt->id!=appliedInterrupt) {
        player.Advance(std::max(0.0,interrupt->age-time));time=interrupt->age;
        player.Interrupt(interrupt->label);appliedInterrupt=interrupt->id;
    }
    player.Advance(std::max(0.0,age-time));time=age;
}
} // namespace koumakyou::battle
