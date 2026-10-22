// 本文件检查真实 ANM 的固定帧序、循环、中断、全部目录和原始 DDS 解码，无需图形窗口。
#include "resources/battle_catalog.hpp"
#include "resources/dds_image.hpp"
#include "stages/stage1/reference_script.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

using namespace koumakyou;
namespace {
const std::filesystem::path kAssets=KOUMAKYOU_ASSET_SOURCE_ROOT;
/// @brief 使用原妖精 4 帧切换/32 帧循环与露米娅 8 帧切换数据验证，而不是检查实现内部计数器。
void TestRealTiming() {
    auto fairy=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Stage1Enemies).anm);
    anm::Player player;player.Start(fairy,0);assert(player.Sample().sprite==0);
    player.Advance(3.0/60);assert(player.Sample().sprite==0);
    player.Advance(1.0/60);assert(player.Sample().sprite==1);
    player.Advance(28.0/60);assert(player.Sample().sprite==0);
    anm::Player fine;fine.Start(fairy,0);
    for(int i=0;i<256;++i)fine.Advance(1.0/240);
    player.Advance(32.0/60);assert(fine.Sample().sprite==player.Sample().sprite);
    auto rumia=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Rumia).anm);
    player.Start(rumia,128);assert(player.Sample().sprite==132);
    player.Advance(8.0/60);assert(player.Sample().sprite==133);
    player.Advance(24.0/60);assert(player.Sample().sprite==132);
    battle::Cursor cursor;cursor.Sync(rumia,{battle::Atlas::Rumia,128},8.0/60);
    const auto sprite=cursor.player.Sample().sprite;
    cursor.Sync(rumia,{battle::Atlas::Rumia,128},8.0/60);assert(cursor.player.Sample().sprite==sprite);
    cursor.Sync(rumia,{battle::Atlas::Rumia,129},0,1);assert(cursor.player.Sample().sprite==135);
    bool rejected=false;try {player.Advance(-1);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
}
/// @brief 读取所有未来关卡、自机和特效；每个真实脚本运行 120 帧，执行停止标签或中断后再推进。
void TestAllNativeAssets() {
    std::size_t sprites=0,scripts=0;
    // 原包 _4k 图集宽高为标准版两倍；少数 ANM 逻辑尺寸与标准 DDS 本就不同，不能直接乘逻辑尺寸。
    constexpr std::array<Vector2,25> expectedTextureSizes{{
        {2048,2048},{256,256},{1024,1024},{2048,2048},{2048,2048},{1024,1024},
        {2048,2048},{4096,4096},{2048,2048},{2048,2048},{4096,4096},{4096,4096},
        {2048,2048},{4096,4096},{4096,4096},{4096,4096},{4096,4096},{4096,4096},
        {1152,1152},{1152,1152},{1152,1152},{826,814},{1152,1152},{1152,1152},{2048,2048}
    }};
    for(const auto& definition:battle::Catalog()) {
        auto archive=anm::LoadArchive(kAssets/definition.anm);
        sprites+=archive.sprites.size();scripts+=archive.scripts.size();
        Image image=LoadOriginalDds(kAssets/definition.dds);
        assert(image.data && image.width>0 && image.height>0 && image.format==PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        const auto size=expectedTextureSizes.at(static_cast<std::size_t>(definition.id));
        assert(image.width==size.x && image.height==size.y);
        UnloadImage(image);
        for(const auto& script:archive.scripts) {
            anm::Player player;player.Start(archive,script.id,0,archive.sprites.front().id,7);
            player.Advance(2);
            for(const auto& instruction:script.instructions)if(instruction.opcode==22) {
                player.Interrupt(instruction.Int(0));player.Advance(0.5);
            }
            assert(player.Sample().sprite>=0 && archive.FindSprite(player.Sample().sprite));
        }
    }
    assert(battle::Catalog().size()==25 && sprites==1084 && scripts==347);
    std::cout << "Validated 25 original DDS/ANM atlases, " << sprites << " sprites, " << scripts << " scripts\n";
}
/// @brief 验证颜色映射、显现期间变换和 ECL 动作重启不再丢失。
void TestBulletAndActorBindings() {
    auto bullets=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Bullets).anm);
    for(int type=0;type<10;++type)for(int color=0;color<16;++color) {
        const auto selection=battle::EnemyBullet(type,color);
        auto archive=anm::LoadArchive(kAssets/battle::Describe(selection.atlas).anm);
        anm::Player player;player.Start(archive,selection.script,selection.spriteOffset);player.Advance(0.5);
        assert(player.Sample().sprite>=0);
    }
    const auto red=battle::EnemyBullet(2,2);
    anm::Player player;player.Start(bullets,red.script,red.spriteOffset);
    assert(player.Sample().sprite==48 && player.Sample().autoRotate==1);
    auto spawn=battle::BulletSpawn(0,2,10);player.Start(bullets,spawn.script,spawn.spriteOffset);
    const auto scale=player.Sample().scale.x;player.Advance(5.0/60);
    assert(player.Sample().scale.x!=scale);player.Advance(5.0/60);assert(player.Finished());
    auto system=std::make_unique<BulletSystem>();stage1::ReferenceScript actor;
    actor.Reset(3,{100,0,0},100,7);actor.Update(1.0F/60,{},*system);
    assert(actor.animationScript==3);const auto revision=actor.animationRevision;
    actor.Update(1,{},*system);assert(actor.animationScript==5 && actor.animationRevision>revision);
    assert(actor.deathEffect==0);
    actor.Reset(0,{100,0,0},100,7);actor.Update(1.0F/60,{},*system);
    assert(actor.deathEffect==1);
}
/// @brief 验证渐变、双位移通道和脚本外部标签；这些效果不能退化为静态裁图。
void TestEffectSemantics() {
    auto archive=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Reimu).anm);
    anm::Player leftOrb,rightOrb;
    leftOrb.Start(archive,128);rightOrb.Start(archive,129);
    assert(leftOrb.Sample().sprite==128 && rightOrb.Sample().sprite==128);
    leftOrb.Advance(8.0/60);rightOrb.Advance(8.0/60);
    assert(leftOrb.Sample().visible && rightOrb.Sample().visible);
    assert(leftOrb.Sample().rotation.z>0 && rightOrb.Sample().rotation.z<0);
    assert(std::abs(leftOrb.Sample().rotation.z+rightOrb.Sample().rotation.z)<0.0001F);
    anm::Player needle;needle.Start(archive,66);needle.Advance(0.2);
    assert(needle.Sample().sprite==68 && needle.Sample().source.width==32 && needle.Sample().source.height==104);
    assert(needle.Sample().visible && needle.Sample().autoRotate==1 && needle.Sample().color.a==160);
    anm::Player effect;effect.Start(archive,137);
    const auto initialAlpha=effect.Sample().color.a;
    effect.Advance(30.0/60);
    assert(effect.Sample().color.a>initialAlpha && effect.Sample().color.a<255);
    effect.Advance(60.0/60);
    assert(effect.Sample().position.x==0 && effect.Sample().positionOffset.x<0);
    effect.Advance(90.0/60);assert(effect.Finished() && !effect.Sample().visible);

    auto aura=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Stage6Enemies).anm);
    effect.Start(aura,16);effect.Advance(2);
    assert(!effect.Finished() && effect.Sample().color.a==255); // halt 保持光环，不能自行跳到删除标签。
    assert(!effect.Interrupt(999));
    assert(effect.Interrupt(1));effect.Advance(1);
    assert(effect.Sample().color.a>0 && effect.Sample().color.a<255);
    effect.Advance(1);assert(effect.Finished() && !effect.Sample().visible);
}
/// @brief 损坏的精灵指针、跳转边界和截断纹理必须明确失败，不能越界读取或上传错误数据。
void TestCorruptFiles() {
    const auto name="koumakyou-native-invalid-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=std::filesystem::temp_directory_path()/name;
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code ignored;std::filesystem::remove(path,ignored);} } cleanup{path};
    const auto& definition=battle::Describe(battle::Atlas::Stage1Enemies);
    std::ifstream stream(kAssets/definition.anm,std::ios::binary);
    const std::vector<unsigned char> original{std::istreambuf_iterator<char>(stream),{}};
    const auto reject=[&](const std::vector<unsigned char>& bytes,bool dds=false) {
        {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
        bool failed=false;
        try {
            if(dds) {Image image=LoadOriginalDds(path);UnloadImage(image);}
            else (void)anm::LoadArchive(path);
        } catch(const std::runtime_error&) {failed=true;}
        assert(failed);
    };
    auto bytes=original;bytes[64]=bytes[65]=bytes[66]=bytes[67]=255;reject(bytes);
    auto archive=anm::LoadArchive(kAssets/definition.anm);
    const auto& instructions=archive.FindScript(0)->instructions;
    const auto jump=std::find_if(instructions.begin(),instructions.end(),[](const auto& op){return op.opcode==5;});
    assert(jump!=instructions.end());
    const auto word=[&](std::size_t at) {return unsigned(original[at])|unsigned(original[at+1])<<8|unsigned(original[at+2])<<16|unsigned(original[at+3])<<24;};
    const auto start=word(64+4*word(0)+4);
    bytes=original;bytes[start+jump->offset+4]=1;reject(bytes); // 跳到指令头中间。
    std::ifstream dds(kAssets/definition.dds,std::ios::binary);
    bytes.resize(128);dds.read(reinterpret_cast<char*>(bytes.data()),bytes.size());reject(bytes,true);
}
}
/// @brief 由核心测试入口调用；任何原始资源损坏或动画契约回归都会使测试失败。
void RunAnimationTests() { TestRealTiming();TestAllNativeAssets();TestBulletAndActorBindings();TestEffectSemantics();TestCorruptFiles(); }
