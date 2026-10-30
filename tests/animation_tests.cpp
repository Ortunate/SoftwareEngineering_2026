// 本文件检查真实 ANM 的固定帧序、循环、中断、全部目录和原始 DDS 解码，无需图形窗口。
#include "resources/battle_catalog.hpp"
#include "resources/dds_image.hpp"
#include "gameplay/actors/behavior.hpp"
#include "stages/common/ids.hpp"
#include "render/animated_sprite.hpp"
#include "reference_script.hpp"
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
/// @brief 四/五面本体中断有 30/120 帧两种淡出，命令按真实年龄交付且不能每次同步重启淡出。
void TestActorInterruptTiming() {
    const auto archive=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Stage4Enemies).anm);
    for(int script:{13,16})for(int fps:{30,60,120,144,240}) {
        const int duration=script==13 ? 30 : 120;
        battle::Cursor cursor;const battle::AnimationInterrupt interrupt{2,1,7,1};
        for(int frame=0;frame<=fps*4;++frame) {
            const double age=static_cast<double>(frame)/fps;
            cursor.Sync(archive,{battle::Atlas::Stage4Enemies,script},age,7,age>=2 ? &interrupt : nullptr);
            if(age>=2&&age<2.1)assert(cursor.player.Sample().color.a<255&&cursor.player.Sample().color.a>180);
            if(age>=2+static_cast<double>(duration)/60)assert(cursor.player.Finished()&&!cursor.player.Sample().visible);
        }
        // 延后交付仍按事件年龄重新构建，不把已经过去的淡出再延长一段。
        battle::Cursor delayed;delayed.Sync(archive,{battle::Atlas::Stage4Enemies,script},4,7);
        delayed.Sync(archive,{battle::Atlas::Stage4Enemies,script},4,7,&interrupt);assert(delayed.player.Finished());
        delayed.Sync(archive,{battle::Atlas::Stage4Enemies,script},0,8,&interrupt);assert(!delayed.player.Finished());
    }
}

/// @brief 美铃66是60帧扩散圈：源脚本独立给出94号精灵、0.3→6.3缩放和255→0透明度，不借本体时间。
void TestMeilingPreparationAnimation() {
    const auto archive=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Stage3Enemies).anm);
    for(int fps:{30,60,120,144,240}) {
        anm::Player player;player.Start(archive,66);
        for(int update=0;update<=fps;++update) {
            if(update)player.Advance(1.0/fps);
            const int frame=update*60/fps;
            const auto& sample=player.Sample();
            if(frame>=60) {assert(player.Finished()&&!sample.visible);continue;}
            const float progress=(frame+1)/60.0F;
            assert(!player.Finished()&&sample.visible&&sample.sprite==94&&sample.autoRotate==1&&sample.disableZWrite);
            assert(!sample.additive&&sample.color.r==255&&sample.color.g==255&&sample.color.b==255);
            assert(sample.color.a==static_cast<unsigned char>(255.0F-255.0F*progress));
            assert(std::abs(sample.scale.x-(.3F+(6.3F-.3F)*progress))<1e-5F&&sample.scale.x==sample.scale.y);
            assert(std::abs(sample.rotation.x+1.04719758F)<1e-6F);
        }
    }
}

behavior::Routine AuxiliaryLifecycle(behavior::Actor& actor) {
    actor.Animate(13);actor.AnimateAuxiliary(0,16);actor.AnimateAuxiliary(7,17);
    co_await actor.At(60);actor.InterruptAuxiliary(0,1);
    co_await actor.At(70);actor.AnimateAuxiliary(7,16);
    co_await actor.At(180);actor.finished=true;
}
/// @brief 以真实Extra ANM直接播放作为独立期望，检查两槽并行、60帧中断、重启与Actor复用解除借用。
void TestAuxiliaryActors() {
    const auto archive=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::ExtraEnemies).anm);
    for(int fps:{30,60,120,144,240}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({},1,7);actor.Start(AuxiliaryLifecycle(actor));
        for(int frame=0;frame<fps*3;++frame) {
            actor.Update(1.0F/fps,{},*bullets);
            const double age=actor.phaseFrames/60;
            if(actor.phaseFrames==0) {
                // 高显示帧率的首个显示帧尚未达到120 Hz逻辑步；此时本体和辅助脚本都应保持未启动。
                assert(actor.animationScript<0&&actor.auxiliaryAnimations[0].script<0&&actor.auxiliaryAnimations[7].script<0);
                continue;
            }
            for(int slot:{0,7}) {
                auto& auxiliary=actor.auxiliaryAnimations[slot];auxiliary.Sync(archive,{battle::Atlas::ExtraEnemies,13});
                const bool restarted=slot==7&&actor.phaseFrames>70;
                anm::Player expected;expected.Start(archive,restarted?16:slot==0?16:17);
                if(slot==0&&actor.phaseFrames>60) {
                    expected.Advance(1);assert(expected.Interrupt(1));expected.Advance(age-1);
                    assert(auxiliary.interrupt&&std::abs(auxiliary.interrupt->age-1)<1e-6);
                } else expected.Advance(restarted?age-70.0/60:age);
                const auto& actual=auxiliary.cursor.player.Sample();const auto& wanted=expected.Sample();
                if(actual.sprite!=wanted.sprite||actual.visible!=wanted.visible||actual.color.a!=wanted.color.a)
                    std::cerr<<"Auxiliary fps "<<fps<<" frame "<<frame<<" slot "<<slot<<" phase "<<actor.phaseFrames
                             <<" age "<<auxiliary.age<<" sprite "<<actual.sprite<<"/"<<wanted.sprite
                             <<" alpha "<<int(actual.color.a)<<"/"<<int(wanted.color.a)<<" visible "<<actual.visible<<"/"<<wanted.visible<<'\n';
                assert(actual.sprite==wanted.sprite&&actual.visible==wanted.visible&&actual.color.a==wanted.color.a);
                assert(auxiliary.cursor.player.Finished()==expected.Finished());
                const double before=auxiliary.cursor.time;auxiliary.Sync(archive,{battle::Atlas::ExtraEnemies,13});
                assert(auxiliary.cursor.time==before&&auxiliary.cursor.player.Sample().color.a==actual.color.a);
            }
            assert(actor.animationScript==13); // 辅助重启/中断不能修改本体动作。
        }
        assert(actor.active&&!actor.finished); // 辅助动画完成不能提前结束本体协程。
        const auto oldRevision=actor.auxiliaryAnimations[0].revision;
        actor.AnimateAuxiliary(0,16);auto& restarted=actor.auxiliaryAnimations[0];
        assert(restarted.age==0&&restarted.revision==oldRevision+1&&!restarted.interrupt);
        restarted.Sync(archive,{battle::Atlas::ExtraEnemies,13});
        anm::Player initial;initial.Start(archive,16);
        assert(restarted.cursor.player.Sample().sprite==initial.Sample().sprite&&restarted.cursor.player.Sample().color.a==initial.Sample().color.a);
        assert(!restarted.cursor.player.Finished());
        behavior::Actor baseline;baseline.Reset({},1,7);assert(actor.Random(1)==baseline.Random(1));
        actor.Reset({},1,9);
        for(const auto& auxiliary:actor.auxiliaryAnimations)
            assert(auxiliary.script<0&&auxiliary.visual==kInvalidVisualId&&!auxiliary.interrupt&&auxiliary.revision==0&&auxiliary.cursor.player.Source()==nullptr);
    }
    behavior::Actor actor;
    for(int slot:{-1,8}) {
        bool rejected=false;try {actor.AnimateAuxiliary(slot,16);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    bool invalidScript=false;try {actor.AnimateAuxiliary(0,-1);}catch(const std::invalid_argument&) {invalidScript=true;}assert(invalidScript);
    bool rejected=false;try {actor.InterruptAuxiliary(0,1);}catch(const std::logic_error&) {rejected=true;}assert(rejected);
    rejected=false;try {actor.InterruptAuxiliary(8,1);}catch(const std::out_of_range&) {rejected=true;}assert(rejected);
    auto bullets=std::make_unique<BulletSystem>();actor.Reset({},1,7);actor.Start(AuxiliaryLifecycle(actor));actor.phaseClockPaused=true;
    actor.Update(1.0F/60,{},*bullets);assert(actor.phaseFrames==0&&std::abs(actor.auxiliaryAnimations[0].age-1.0/60)<1e-6);
    // 芙兰朵露160在另一张图集，光圈18只存在ExtraEnemies；选择和重启均须保留/清除明确的外观归属。
    const auto body=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Flandre).anm);
    assert(body.FindScript(160)&&!body.FindScript(18)&&archive.FindScript(18));
    // Kagome Kagome的167自身保留64×80可见帧，但演员132另行隐藏本体；回收仍按精灵半尺寸。
    anm::Player emitter;emitter.Start(body,167);
    for(int frame=0;frame<300;++frame) {
        assert(emitter.Sample().visible&&emitter.Sample().sprite==160);
        assert(emitter.Sample().LogicalSize().x==64&&emitter.Sample().LogicalSize().y==80);
        emitter.Advance(1.0/60);
    }
    actor.Animate(160);actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
    auto& cross=actor.auxiliaryAnimations[0];cross.Sync(archive,{battle::Atlas::ExtraEnemies,13});
    assert(cross.visual==stage_assets::kOrb&&cross.cursor.selection.atlas==battle::Atlas::ExtraEnemies&&cross.cursor.player.Source()==&archive);
    actor.AnimateAuxiliary(0,16);assert(cross.visual==kInvalidVisualId); // 缺省重启必须恢复沿用本体，不能遗留旧图集槽。
}

/// @brief 按已审阅的 ANM 显式尺寸和 EXE 判定常量验收全部颜色，覆盖大泡 C4 与火球换帧。
void TestNativeBulletLogicalSize() {
    constexpr std::array<Vector2,10> sizes{{{8,8},{16,16},{14,16},{16,16},{14,16},{14,16},{32,32},{30,30},{32,32},{64,64}}};
    constexpr std::array<float,10> radii{2,3,2,3,2.5F,2,8,5.5F,4.5F,16};
    for(int type=0;type<10;++type)for(int color=0;color<16;++color) {
        const auto selection=battle::EnemyBullet(type,color);const auto archive=anm::LoadArchive(kAssets/battle::Describe(selection.atlas).anm);
        anm::Player player;player.Start(archive,selection.script,selection.spriteOffset);
        const Vector2 expected=type==9&&color%5==4 ? Vector2{30,30} : sizes[type];
        const Vector2 actual=battle::EnemyBulletLogicalSize(type,color);
        for(int frame=0;frame<20;++frame) {
            const auto display=player.Sample().LogicalSize();
            assert(display.x==expected.x&&display.y==expected.y);
            player.Advance(1.0/60);
        }
        assert(std::abs(actual.x-expected.x*(20.0F/448))<1e-6F);
        assert(std::abs(actual.y-expected.y*(20.0F/448))<1e-6F);
        assert(std::abs(battle::EnemyBulletHitRadius(type)-radii[type]*(20.0F/448))<1e-6F);
    }
    bool rejected=false;try {(void)battle::EnemyBulletLogicalSize(10);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    rejected=false;try {(void)battle::EnemyBulletLogicalSize(9,-1);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    rejected=false;try {(void)battle::EnemyBulletHitRadius(-1);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
}
/// @brief 独立记录各角色/自机的原包基础尺寸，检查逻辑比例而不是高清裁图比例。
void TestNativeActorAndPlayerGeometry() {
    struct Case {battle::Atlas atlas;int script;float width,height;};
    const std::array<Case,13> cases{{
        {battle::Atlas::Stage1Enemies,0,30,30},{battle::Atlas::Rumia,128,192*4.0F/9,192*4.0F/9},
        {battle::Atlas::Stage2Enemies,64,161.5F*4/9,161.5F*4/9},
        {battle::Atlas::Cirno,128,218*4.0F/9,218*4.0F/9},
        {battle::Atlas::Stage3Enemies,64,159*4.0F/9,159*4.0F/9},
        {battle::Atlas::Stage4Enemies,64,154*4.0F/9,154*4.0F/9},
        {battle::Atlas::Stage4Enemies,65,154*4.0F/9,154*4.0F/9},
        {battle::Atlas::Sakuya,128,92,92},{battle::Atlas::Remilia,160,230*4.0F/9,230*4.0F/9},
        {battle::Atlas::Reimu,64,14,14},{battle::Atlas::Reimu,66,14,46},
        {battle::Atlas::Reimu,128,16,16},{battle::Atlas::Marisa,128,16,16}
    }};
    const Camera3D camera{{0,0,-10},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    for(const auto& test:cases) {
        const auto archive=anm::LoadArchive(kAssets/battle::Describe(test.atlas).anm);
        anm::Player player;player.Start(archive,test.script);auto frame=player.Sample();
        const auto logical=frame.LogicalSize();
        assert(std::abs(logical.x-test.width)<1e-5F&&std::abs(logical.y-test.height)<1e-5F);
        frame.rotation={};frame.position=frame.positionOffset={};frame.scale={1,1};
        const auto vertices=AnimatedBillboardVertices(camera,frame,{},test.height*(20.0F/448));
        assert(std::abs(std::abs(vertices[1].x-vertices[0].x)-test.width*(20.0F/448))<1e-5F);
        assert(std::abs(vertices[0].y-vertices[3].y-test.height*(20.0F/448))<1e-5F);
        // 改变贴图裁切尺寸只改变 UV，显式显示尺寸和实际世界边界不能跟着变化。
        frame.source.width*=2;frame.source.height*=2;
        const auto hires=AnimatedBillboardVertices(camera,frame,{},test.height*(20.0F/448));
        for(int i=0;i<4;++i)assert(hires[i].x==vertices[i].x&&hires[i].y==vertices[i].y&&hires[i].z==vertices[i].z);
    }
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
    // 独立检查原始17/18/19脚本终止时刻，不从BulletSpawn或弹池配置反推显现长度。
    for(const auto [script,frame]:std::array<std::pair<int,int>,3>{{{17,10},{18,16},{19,32}}}) {
        const auto* original=bullets.FindScript(script);assert(original);
        auto end=std::find_if(original->instructions.begin(),original->instructions.end(),[](const anm::Instruction& instruction){return instruction.opcode==0;});
        assert(end!=original->instructions.end()&&end->frame==frame);
    }
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
    auto marisa=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::Marisa).anm);
    for(int script:{64,65,66,67,68,69,70,71,97,98,99,100,128,129}) {
        anm::Player effect;effect.Start(marisa,script);effect.Advance(0.2);
        assert(effect.Sample().visible && effect.Sample().sprite>=0);
        if(script==128 || script==129)assert(effect.Sample().sprite==128 && effect.Sample().rotation.z>0);
        if(script>=69 && script<=71) {
            assert(effect.Sample().sprite==68 && effect.Sample().color.a>0);
            effect.Advance(1);const auto alpha=effect.Sample().color.a;
            assert(effect.Interrupt(1));effect.Advance(15.0/60);
            assert(effect.Sample().color.a<alpha);effect.Advance(16.0/60);assert(effect.Finished());
        }
        if(script>=97 && script<=100) {effect.Advance(0.2);assert(effect.Finished());}
    }
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
/// @brief 原生装载etama4基址698，705/706实际对应7/8脚本，寿命为60/240内容帧。
/// 按原始脚本独立核对缩放和淡入，不把旧eff00候选素材的30帧删除当作本版本答案。
void TestChargeEffectAnimations() {
    const auto archive=anm::LoadArchive(kAssets/battle::Describe(battle::Atlas::BulletEffects).anm);
    // 漂散20对应局部21：随机7…10、加法混合、禁深度写、缩小并旋转120内容帧。
    const auto* drift=archive.FindScript(21);assert(drift&&drift->instructions.size()==9);
    assert(drift->instructions.front().opcode==16&&drift->instructions.front().Int(0)==7&&drift->instructions.front().Int(1)==4);
    assert(drift->instructions.back().opcode==0&&drift->instructions.back().frame==120);
    for(int fps:{30,60,120,144,240}) {
        anm::Player player;player.Start(archive,21);const int sprite=player.Sample().sprite;
        assert(sprite>=7&&sprite<=10);
        for(int frame=0;frame<120;++frame) {
            const auto& sample=player.Sample();assert(!player.Finished()&&sample.visible&&sample.sprite==sprite&&sample.additive&&sample.disableZWrite);
            const float time=static_cast<float>(frame+1);
            assert(std::abs(sample.scale.x-(1.5F-.0125F*time))<2e-5F&&std::abs(sample.scale.y-sample.scale.x)<1e-5F);
            assert(std::abs(sample.rotation.z-std::remainder(.15F*time,2*PI))<3e-5F);
            assert(sample.color.a==static_cast<unsigned char>(128*(1-time/120)));
            player.Advance(1.0/60);
        }
        assert(player.Finished());player.Start(archive,21);
        for(int tick=0;tick<fps*2;++tick)player.Advance(1.0/fps);
        assert(player.Finished());
    }
    // 擦弹8对应局部13，不能沿用漂散21的缩放、角速度或120帧寿命。
    const auto* graze=archive.FindScript(13);assert(graze&&graze->instructions.size()==9);
    assert(graze->instructions.front().opcode==16&&graze->instructions.front().Int(0)==7&&graze->instructions.front().Int(1)==4);
    assert(graze->instructions.back().opcode==0&&graze->instructions.back().frame==30);
    anm::Player grazePlayer;grazePlayer.Start(archive,13);const int grazeSprite=grazePlayer.Sample().sprite;
    assert(grazeSprite>=7&&grazeSprite<=10);
    for(int frame=0;frame<30;++frame) {
        const auto& sample=grazePlayer.Sample();const float time=static_cast<float>(frame+1);
        assert(!grazePlayer.Finished()&&sample.visible&&sample.sprite==grazeSprite&&sample.additive&&sample.disableZWrite);
        assert(std::abs(sample.scale.x-(.7F-.023333333432674408F*time))<2e-5F&&sample.scale.x==sample.scale.y);
        assert(std::abs(sample.rotation.z-std::remainder(.6F*time,2*PI))<3e-5F);
        assert(sample.color.a==static_cast<unsigned char>(128*(1-time/30)));
        grazePlayer.Advance(1.0/60);
    }
    assert(grazePlayer.Finished());
    for(int fps:{30,60,120,144,240}) {
        grazePlayer.Start(archive,13);
        for(int tick=0;tick<fps/2;++tick)grazePlayer.Advance(1.0/fps);
        assert(grazePlayer.Finished()&&!grazePlayer.Sample().visible);
    }
    for(int script:{7,8}) {
        const auto* source=archive.FindScript(script);assert(source);
        assert(source->instructions.size()==8);
        assert(source->instructions.front().opcode==1&&source->instructions.front().Int(0)==6);
        const int lifetime=script==7?60:240,fade=script==7?40:120,alpha=script==7?128:48;
        const auto& end=source->instructions.back();assert(end.frame==lifetime&&end.opcode==0);
        {
            anm::Player player;player.Start(archive,script);
            for(int frame=0;frame<lifetime;++frame) {
                const auto& sample=player.Sample();
                assert(!player.Finished()&&sample.visible&&sample.sprite==6);
                assert(sample.additive&&sample.disableZWrite);
                assert(sample.color.r==255&&sample.color.g==255&&sample.color.b==255);
                // Start立即执行帧0；7/8每帧分别缩0.05/0.0125，向128/48淡入后保持，不发生旋转。
                const float time=static_cast<float>(frame+1);
                const float wanted=3.5F-(script==7?.05F:.0125F)*time;
                assert(std::abs(sample.scale.x-wanted)<1e-5F&&std::abs(sample.scale.y-wanted)<1e-5F);
                assert(sample.rotation.z==0);
                assert(sample.color.a==static_cast<unsigned char>(alpha*std::min(1.0F,time/fade)));
                player.Advance(1.0/60);
            }
            assert(player.Finished()&&!player.Sample().visible);
        }
        for(int fps:{30,60,120,144,240}) {
            anm::Player player;player.Start(archive,script);
            for(int tick=0;tick<fps*lifetime/60;++tick)player.Advance(1.0/fps);
            assert(player.Finished()&&!player.Sample().visible);
        }
    }
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
    const auto spriteAt=unsigned(original[64])|unsigned(original[65])<<8|unsigned(original[66])<<16|unsigned(original[67])<<24;
    for(const auto bits:{0xbf800000U,0x7fc00000U}) {
        auto invalidSize=original;
        for(int i=0;i<4;++i)invalidSize[spriteAt+20+i]=static_cast<unsigned char>(bits>>(i*8));
        reject(invalidSize);
    }
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
void RunAnimationTests() { TestRealTiming();TestActorInterruptTiming();TestMeilingPreparationAnimation();TestAuxiliaryActors();TestNativeBulletLogicalSize();TestNativeActorAndPlayerGeometry();TestAllNativeAssets();TestBulletAndActorBindings();TestEffectSemantics();TestChargeEffectAnimations();TestCorruptFiles(); }
/// @brief 单独定位辅助槽的时钟、中断和复用，不运行其他关卡源差分。
void RunAuxiliaryAnimationTests() {TestMeilingPreparationAnimation();TestAuxiliaryActors();}
/// @brief 单独检查外部蓄力粒子的真实ANM，为外部效果池的独立接入提供素材边界。
void RunNativeEffectAnimationTests() {TestChargeEffectAnimations();}
