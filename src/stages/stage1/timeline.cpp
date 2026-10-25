// 本文件直接定义一面固定 StageEvent 表，包含逐只出生、道中 Boss、战前/战后对话和关底 Boss。
#include "stages/stage1/data.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/ids.hpp"
#include <array>

namespace koumakyou::stage1 {
namespace {
/// @brief 仅将显式出生参数封装为单只出怪动作，不查询原始数值目录或推导出怪时刻。
/// position 为世界坐标，health/score 为原始点数，item 为 -1/0；mirrored 反转 X 位移。
/// randomX 在演员出生时求值一次，启用时 position.x=0 只作占位，不是最终横坐标。
/// seed 为固定实例种子，不依赖本表行号；visual 属于一面素材表，pattern 按值复制。
SpawnActors Spawn(Vector3 position,const behavior::Pattern& pattern,float health,int score,int item,
                   bool mirrored,bool randomX,unsigned seed,VisualId visual) {
    return {behavior::Spawn{.position=position,.pattern=pattern,
        .mirrored=mirrored,.randomX=randomX,.sourceHealth=health,.sourceScore=score,
        .seed=seed,.sourceItem=item,.visual=visual,.hitRadius=0.625F,
        .bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,.defeatBase=kNativeDefeat,.fallback=kBlueBullet},1,0};
}

// atFrame 为开局起算的 60 Hz 绝对帧；同帧事件顺序也是内容的一部分。
// 道中 Boss 期间关卡时钟继续推进，到期的杂鱼请求由 Game 跳过。
const std::array<StageEvent,226> kEvents{{

    {.atFrame=100,.action=Spawn({-5.89285707F,0.0F,11.4285717F},kTurningFairy,8,300,-1,false,false,3100U,kFairy)},
    {.atFrame=116,.action=Spawn({-5.53571463F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,false,3597U,kFairy)},
    {.atFrame=132,.action=Spawn({-5.1785717F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,false,4094U,kFairy)},
    {.atFrame=148,.action=Spawn({-4.82142878F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,false,4591U,kFairy)},
    {.atFrame=164,.action=Spawn({-4.46428585F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,false,5088U,kFairy)},

    {.atFrame=228,.action=Spawn({5.89285707F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,7073U,kFairy)},
    {.atFrame=244,.action=Spawn({5.53571463F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,7570U,kFairy)},
    {.atFrame=260,.action=Spawn({5.1785717F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,8067U,kFairy)},
    {.atFrame=276,.action=Spawn({4.82142878F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,8564U,kFairy)},
    {.atFrame=292,.action=Spawn({4.46428585F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,9061U,kFairy)},
    {.atFrame=308,.action=Spawn({4.10714293F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,9558U,kFairy)},
    {.atFrame=324,.action=Spawn({3.75F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,10055U,kFairy)},
    {.atFrame=340,.action=Spawn({3.39285731F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,false,10552U,kFairy)},

    {.atFrame=390,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,12103U,kFairy)},
    {.atFrame=390,.action=Spawn({7.14285755F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,12104U,kFairy)},
    {.atFrame=406,.action=Spawn({-6.4285717F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,12601U,kFairy)},
    {.atFrame=406,.action=Spawn({6.4285717F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,12602U,kFairy)},
    {.atFrame=422,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,13099U,kFairy)},
    {.atFrame=422,.action=Spawn({5.71428585F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,13100U,kFairy)},
    {.atFrame=438,.action=Spawn({-5.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,13597U,kFairy)},
    {.atFrame=438,.action=Spawn({5.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,13598U,kFairy)},
    {.atFrame=454,.action=Spawn({-4.28571415F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,14095U,kFairy)},
    {.atFrame=454,.action=Spawn({4.28571415F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,14096U,kFairy)},
    {.atFrame=470,.action=Spawn({-3.57142878F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,14593U,kFairy)},
    {.atFrame=470,.action=Spawn({3.57142878F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,14594U,kFairy)},
    {.atFrame=486,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,15091U,kFairy)},
    {.atFrame=486,.action=Spawn({2.85714293F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,15092U,kFairy)},
    {.atFrame=502,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,15589U,kFairy)},
    {.atFrame=502,.action=Spawn({2.14285707F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,15590U,kFairy)},
    {.atFrame=518,.action=Spawn({-1.42857146F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,16087U,kFairy)},
    {.atFrame=518,.action=Spawn({1.42857146F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,false,16088U,kFairy)},
    {.atFrame=534,.action=Spawn({-0.714285731F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,false,16585U,kFairy)},
    {.atFrame=534,.action=Spawn({0.714285731F,0.0F,11.4285717F},kLateTurningFairy,8,300,-1,true,false,16586U,kFairy)},

    {.atFrame=594,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,false,false,18447U,kDarkFairy)},

    {.atFrame=654,.action=Spawn({2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,true,false,20308U,kDarkFairy)},
    {.atFrame=704,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,false,false,21859U,kDarkFairy)},

    {.atFrame=754,.action=Spawn({7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,true,false,23410U,kDarkFairy)},

    {.atFrame=804,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,false,false,24961U,kDarkFairy)},
    {.atFrame=844,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,true,false,26202U,kDarkFairy)},

    {.atFrame=884,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,false,false,27443U,kDarkFairy)},

    {.atFrame=914,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,true,false,28374U,kDarkFairy)},
    {.atFrame=944,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,false,false,29305U,kDarkFairy)},

    {.atFrame=974,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,true,false,30236U,kDarkFairy)},

    {.atFrame=994,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,false,false,30857U,kDarkFairy)},
    {.atFrame=1014,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,true,false,31478U,kDarkFairy)},

    {.atFrame=1034,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,false,false,32099U,kDarkFairy)},

    {.atFrame=1044,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,0,true,false,32410U,kDarkFairy)},

    {.atFrame=1054,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,false,false,32721U,kDarkFairy)},
    {.atFrame=1064,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,0,true,false,33032U,kDarkFairy)},

    {.atFrame=1174,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,true,36443U,kFairy)},
    {.atFrame=1184,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,36754U,kFairy)},

    {.atFrame=1194,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,true,37065U,kFairy)},

    {.atFrame=1204,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,37376U,kFairy)},

    {.atFrame=1214,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,true,37687U,kFairy)},

    {.atFrame=1224,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,37998U,kFairy)},
    {.atFrame=1234,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,true,38309U,kFairy)},

    {.atFrame=1244,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,true,38620U,kFairy)},
    {.atFrame=1254,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,true,38931U,kFairy)},

    {.atFrame=1264,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,39242U,kFairy)},
    {.atFrame=1274,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,39553U,kFairy)},
    {.atFrame=1284,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,true,39864U,kFairy)},
    {.atFrame=1294,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,40175U,kFairy)},

    {.atFrame=1304,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,true,40486U,kFairy)},
    {.atFrame=1314,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,true,40797U,kFairy)},

    {.atFrame=1324,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,true,true,41108U,kFairy)},
    {.atFrame=1334,.action=Spawn({0.0F,0.0F,11.4285717F},kTurningFairy,32,300,-1,false,true,41419U,kFairy)},

    {.atFrame=1344,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,true,true,41730U,kFairy)},
    {.atFrame=1354,.action=Spawn({0.0F,0.0F,11.4285717F},kLateTurningFairy,32,300,-1,false,true,42041U,kFairy)},

    {.atFrame=1554,.action=Spawn({5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,48242U,kFairy)},
    {.atFrame=1554,.action=Spawn({7.14285755F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,48243U,kFairy)},
    {.atFrame=1570,.action=Spawn({5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,48740U,kFairy)},
    {.atFrame=1570,.action=Spawn({6.78571463F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,48741U,kFairy)},
    {.atFrame=1586,.action=Spawn({5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,49238U,kFairy)},
    {.atFrame=1586,.action=Spawn({6.4285717F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,49239U,kFairy)},
    {.atFrame=1602,.action=Spawn({4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,49736U,kFairy)},
    {.atFrame=1602,.action=Spawn({6.07142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,49737U,kFairy)},
    {.atFrame=1618,.action=Spawn({4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,50234U,kFairy)},
    {.atFrame=1618,.action=Spawn({5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,50235U,kFairy)},
    {.atFrame=1634,.action=Spawn({3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,50732U,kFairy)},
    {.atFrame=1634,.action=Spawn({5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,50733U,kFairy)},
    {.atFrame=1650,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,51230U,kFairy)},
    {.atFrame=1650,.action=Spawn({-4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,51231U,kFairy)},
    {.atFrame=1666,.action=Spawn({-5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,51728U,kFairy)},
    {.atFrame=1666,.action=Spawn({-3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,51729U,kFairy)},
    {.atFrame=1682,.action=Spawn({-5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,52226U,kFairy)},
    {.atFrame=1682,.action=Spawn({-3.57142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,52227U,kFairy)},
    {.atFrame=1698,.action=Spawn({-4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,52724U,kFairy)},
    {.atFrame=1698,.action=Spawn({-3.21428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,52725U,kFairy)},
    {.atFrame=1714,.action=Spawn({-4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,53222U,kFairy)},
    {.atFrame=1714,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,53223U,kFairy)},
    {.atFrame=1730,.action=Spawn({-3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,53720U,kFairy)},
    {.atFrame=1730,.action=Spawn({-2.5F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,53721U,kFairy)},
    {.atFrame=1746,.action=Spawn({-3.57142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,54218U,kFairy)},
    {.atFrame=1746,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,54219U,kFairy)},
    {.atFrame=1762,.action=Spawn({-3.21428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,54716U,kFairy)},
    {.atFrame=1762,.action=Spawn({-1.78571439F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,54717U,kFairy)},

    // 道中遭遇独立异步执行，不以其结束时间重排后续波次。
    {.atFrame=1882,.action=StartBoss{},.id=1},

    {.atFrame=2282,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,70839U,kDarkFairy)},

    {.atFrame=2312,.action=Spawn({2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,71770U,kDarkFairy)},
    {.atFrame=2342,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,72701U,kDarkFairy)},

    {.atFrame=2372,.action=Spawn({7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,73632U,kDarkFairy)},

    {.atFrame=2402,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,74563U,kDarkFairy)},
    {.atFrame=2432,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,75494U,kDarkFairy)},

    {.atFrame=2462,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,76425U,kDarkFairy)},

    {.atFrame=2492,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,77356U,kDarkFairy)},
    {.atFrame=2522,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,78287U,kDarkFairy)},

    {.atFrame=2552,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,79218U,kDarkFairy)},

    {.atFrame=2582,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,80149U,kDarkFairy)},
    {.atFrame=2612,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,81080U,kDarkFairy)},

    {.atFrame=2642,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,82011U,kDarkFairy)},

    {.atFrame=2672,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,82942U,kDarkFairy)},

    {.atFrame=2702,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,83873U,kDarkFairy)},
    {.atFrame=2732,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,84804U,kDarkFairy)},
    {.atFrame=2762,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,85735U,kDarkFairy)},

    {.atFrame=2792,.action=Spawn({2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,86666U,kDarkFairy)},
    {.atFrame=2822,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,87597U,kDarkFairy)},

    {.atFrame=2852,.action=Spawn({7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,88528U,kDarkFairy)},

    {.atFrame=2882,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,89459U,kDarkFairy)},
    {.atFrame=2912,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,90390U,kDarkFairy)},

    {.atFrame=2942,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,91321U,kDarkFairy)},

    {.atFrame=2972,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,92252U,kDarkFairy)},
    {.atFrame=3002,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,93183U,kDarkFairy)},

    {.atFrame=3032,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,94114U,kDarkFairy)},

    {.atFrame=3062,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,95045U,kDarkFairy)},
    {.atFrame=3092,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,95976U,kDarkFairy)},

    {.atFrame=3122,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,96907U,kDarkFairy)},

    {.atFrame=3152,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,97838U,kDarkFairy)},

    {.atFrame=3182,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,98769U,kDarkFairy)},
    {.atFrame=3212,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,99700U,kDarkFairy)},
    {.atFrame=3242,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,100631U,kDarkFairy)},

    {.atFrame=3272,.action=Spawn({2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,101562U,kDarkFairy)},
    {.atFrame=3302,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,102493U,kDarkFairy)},

    {.atFrame=3332,.action=Spawn({7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,103424U,kDarkFairy)},

    {.atFrame=3362,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,104355U,kDarkFairy)},
    {.atFrame=3392,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,105286U,kDarkFairy)},

    {.atFrame=3422,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,106217U,kDarkFairy)},

    {.atFrame=3452,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,107148U,kDarkFairy)},
    {.atFrame=3482,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,108079U,kDarkFairy)},

    {.atFrame=3512,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,109010U,kDarkFairy)},

    {.atFrame=3542,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,109941U,kDarkFairy)},
    {.atFrame=3572,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,110872U,kDarkFairy)},

    {.atFrame=3602,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,111803U,kDarkFairy)},

    {.atFrame=3632,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,112734U,kDarkFairy)},

    {.atFrame=3662,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,113665U,kDarkFairy)},
    {.atFrame=3692,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,114596U,kDarkFairy)},
    {.atFrame=3722,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,115527U,kDarkFairy)},

    {.atFrame=3752,.action=Spawn({2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,116458U,kDarkFairy)},
    {.atFrame=3782,.action=Spawn({-2.85714293F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,117389U,kDarkFairy)},

    {.atFrame=3812,.action=Spawn({7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,118320U,kDarkFairy)},

    {.atFrame=3842,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,119251U,kDarkFairy)},
    {.atFrame=3872,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,120182U,kDarkFairy)},

    {.atFrame=3902,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,121113U,kDarkFairy)},

    {.atFrame=3932,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,122044U,kDarkFairy)},
    {.atFrame=3962,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,122975U,kDarkFairy)},

    {.atFrame=3992,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,123906U,kDarkFairy)},

    {.atFrame=4022,.action=Spawn({-7.5F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,false,false,124837U,kDarkFairy)},
    {.atFrame=4052,.action=Spawn({5.0F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,125768U,kDarkFairy)},

    {.atFrame=4082,.action=Spawn({-2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,126699U,kDarkFairy)},

    {.atFrame=4112,.action=Spawn({6.78571463F,0.0F,11.4285717F},kPassingDarkFairy,80,700,-1,true,false,127630U,kDarkFairy)},

    {.atFrame=4142,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,false,false,128561U,kDarkFairy)},
    {.atFrame=4172,.action=Spawn({2.14285707F,0.0F,11.4285717F},kShootingDarkFairy,80,700,-1,true,false,129492U,kDarkFairy)},

    {.atFrame=4372,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,135693U,kFairy)},
    {.atFrame=4372,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,135694U,kFairy)},
    {.atFrame=4388,.action=Spawn({-6.78571463F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,136191U,kFairy)},
    {.atFrame=4388,.action=Spawn({-5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,136192U,kFairy)},
    {.atFrame=4404,.action=Spawn({-6.4285717F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,136689U,kFairy)},
    {.atFrame=4404,.action=Spawn({-5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,136690U,kFairy)},
    {.atFrame=4420,.action=Spawn({-6.07142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,137187U,kFairy)},
    {.atFrame=4420,.action=Spawn({-4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,137188U,kFairy)},
    {.atFrame=4436,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,137685U,kFairy)},
    {.atFrame=4436,.action=Spawn({-4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,137686U,kFairy)},
    {.atFrame=4452,.action=Spawn({-5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,138183U,kFairy)},
    {.atFrame=4452,.action=Spawn({-3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,138184U,kFairy)},
    {.atFrame=4468,.action=Spawn({-5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,138681U,kFairy)},
    {.atFrame=4468,.action=Spawn({-3.57142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,138682U,kFairy)},
    {.atFrame=4484,.action=Spawn({5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,139179U,kFairy)},
    {.atFrame=4484,.action=Spawn({7.14285755F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,139180U,kFairy)},
    {.atFrame=4500,.action=Spawn({5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,139677U,kFairy)},
    {.atFrame=4500,.action=Spawn({6.78571463F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,139678U,kFairy)},
    {.atFrame=4516,.action=Spawn({5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,140175U,kFairy)},
    {.atFrame=4516,.action=Spawn({6.4285717F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,140176U,kFairy)},
    {.atFrame=4532,.action=Spawn({4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,140673U,kFairy)},
    {.atFrame=4532,.action=Spawn({6.07142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,140674U,kFairy)},
    {.atFrame=4548,.action=Spawn({4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,141171U,kFairy)},
    {.atFrame=4548,.action=Spawn({5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,141172U,kFairy)},
    {.atFrame=4564,.action=Spawn({3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,141669U,kFairy)},
    {.atFrame=4564,.action=Spawn({5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,141670U,kFairy)},
    {.atFrame=4580,.action=Spawn({3.57142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,142167U,kFairy)},
    {.atFrame=4580,.action=Spawn({5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,142168U,kFairy)},
    {.atFrame=4596,.action=Spawn({3.21428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,142665U,kFairy)},
    {.atFrame=4596,.action=Spawn({4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,true,false,142666U,kFairy)},
    {.atFrame=4612,.action=Spawn({-7.14285755F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,143163U,kFairy)},
    {.atFrame=4612,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,143164U,kFairy)},
    {.atFrame=4628,.action=Spawn({-6.78571463F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,143661U,kFairy)},
    {.atFrame=4628,.action=Spawn({-5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,143662U,kFairy)},
    {.atFrame=4644,.action=Spawn({-6.4285717F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,144159U,kFairy)},
    {.atFrame=4644,.action=Spawn({-5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,144160U,kFairy)},
    {.atFrame=4660,.action=Spawn({-6.07142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,144657U,kFairy)},
    {.atFrame=4660,.action=Spawn({-4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,144658U,kFairy)},
    {.atFrame=4676,.action=Spawn({-5.71428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,145155U,kFairy)},
    {.atFrame=4676,.action=Spawn({-4.28571415F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,145156U,kFairy)},
    {.atFrame=4692,.action=Spawn({-5.35714293F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,145653U,kFairy)},
    {.atFrame=4692,.action=Spawn({-3.92857146F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,145654U,kFairy)},
    {.atFrame=4708,.action=Spawn({-5.0F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,146151U,kFairy)},
    {.atFrame=4708,.action=Spawn({-3.57142878F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,146152U,kFairy)},
    {.atFrame=4724,.action=Spawn({-4.64285707F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,146649U,kFairy)},
    {.atFrame=4724,.action=Spawn({-3.21428585F,0.0F,11.4285717F},kTurningFairy,3,300,-1,false,false,146650U,kFairy)},

    {.atFrame=4740,.action=Spawn({5.71428585F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,147147U,kFairy)},
    {.atFrame=4740,.action=Spawn({7.14285755F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,147148U,kFairy)},
    {.atFrame=4756,.action=Spawn({5.35714293F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,147645U,kFairy)},
    {.atFrame=4756,.action=Spawn({6.78571463F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,147646U,kFairy)},
    {.atFrame=4772,.action=Spawn({5.0F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,148143U,kFairy)},
    {.atFrame=4772,.action=Spawn({6.4285717F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,148144U,kFairy)},
    {.atFrame=4788,.action=Spawn({4.64285707F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,148641U,kFairy)},
    {.atFrame=4788,.action=Spawn({6.07142878F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,148642U,kFairy)},
    {.atFrame=4804,.action=Spawn({4.28571415F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,149139U,kFairy)},
    {.atFrame=4804,.action=Spawn({5.71428585F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,149140U,kFairy)},
    {.atFrame=4820,.action=Spawn({3.92857146F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,149637U,kFairy)},
    {.atFrame=4820,.action=Spawn({5.35714293F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,149638U,kFairy)},
    {.atFrame=4836,.action=Spawn({3.57142878F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,150135U,kFairy)},
    {.atFrame=4836,.action=Spawn({5.0F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,150136U,kFairy)},
    {.atFrame=4852,.action=Spawn({3.21428585F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,150633U,kFairy)},
    {.atFrame=4852,.action=Spawn({4.64285707F,0.0F,11.4285717F},kGentleTurningFairy,3,300,-1,true,false,150634U,kFairy)},

    // 对话和战斗按实际结束衔接，不能把关底各阶段改成固定墙钟时刻。
    {.atFrame=5092,.action=StartDialogue{},.id=2},
    {.action=StartBoss{BossEncounter::Final},.id=3,.afterEndOf=2},
    {.action=StartDialogue{true},.id=4,.afterEndOf=3},
}};
} // namespace

std::span<const StageEvent> Stage1Timeline() { return kEvents; }
} // namespace koumakyou::stage1
