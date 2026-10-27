// 本文件验证全部导入目录、原始 HP 解释器映射、四机体实际伤害及未实现结算的扩展边界。
#include "actor_fixture.hpp"
#include "gameplay/data/native_combat.hpp"
#include "gameplay/combat/damage_rules.hpp"
#include "gameplay/actors/player.hpp"
#include "stages/stage1/content.hpp"
#include "reference_program.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace koumakyou;
namespace nc=native_combat;
void TestCatalogAndHealth() {
    constexpr std::array<std::size_t,7> counts{289,547,570,706,364,340,782};
    assert(nc::Stages().size()==7 && !nc::Stage(0) && !nc::Stage(8));
    std::size_t total=0;
    for(const auto& stage:nc::Stages()) {
        assert(stage.health.size()==counts[stage.stage-1] && stage.sha256.size()==64);
        assert(stage.integration==(stage.stage<=6 ? nc::Integration::Playable : nc::Integration::DataOnly));
        total+=stage.health.size();
    }
    assert(total==3598 && nc::ExecutableHash().size()==64);
    assert(nc::RequireLiteral(nc::Spawn(1,0,1882,9))==6000);
    assert(nc::RequireLiteral(nc::Setting(1,14,111))==7000);
    assert(nc::RequireLiteral(nc::Setting(1,14,113))==900);
    assert(nc::RequireLiteral(nc::Setting(1,19,111))==7500);
    assert(nc::RequireLiteral(nc::Setting(1,19,113))==800);
    assert(!nc::Instruction(1,9,-1) && !nc::Setting(8,14,111));
    int instructions=0,healthInstructions=0;
    for(std::size_t sub=0;sub<stage1::reference::kPrograms.size();++sub) {
        int index=0;
        for(const auto& instruction:stage1::reference::kPrograms[sub]) {
            ++instructions;
            const bool health=instruction.opcode==105 || instruction.opcode==108 ||
                              (instruction.opcode>=111 && instruction.opcode<=116);
            const auto* record=nc::Instruction(1,sub,index++);
            assert(health==(record!=nullptr));
            if(record) {
                ++healthInstructions;
                assert(record->opcode==instruction.opcode && record->frame==instruction.frame);
                assert(record->value && *record->value==instruction.args[0]);
            }
        }
    }
    assert(instructions==570 && healthInstructions>40);
    // 默认生命和动态寄存器由未来关卡上下文解释，不允许把缺失记录或寄存器编号当 HP。
    nc::HealthRecord unknown{};
    for(int scenario=0;scenario<3;++scenario) {
        unknown.value=scenario==2 ? std::optional<int>(-10001) : std::nullopt;
        unknown.variable=scenario==2;
        bool rejected=false;
        try { (void)nc::RequireLiteral(scenario==0 ? nullptr : &unknown); }
        catch(const std::logic_error&) { rejected=true; }
        assert(rejected);
    }
    stage1::Stage1Content content;
    assert(content.CombatData()==nc::Stage(1) && !content.DamageRules());
    int births=0;
    for(const auto& event:content.Events())if(const auto* spawn=std::get_if<SpawnActors>(&event.action)) {
        const auto* params=&spawn->actor;
        assert(params);
        auto enemy=testing::MakeActor(spawn->actor);
        assert(enemy && enemy->health==params->sourceHealth && enemy->maxHealth==params->sourceHealth);
        ++births;
    }
    assert(births==222);
    auto boss=content.CreateBoss(true);
    assert(boss->health==7000 && boss->maxHealth==7000);
}

// 独立保存四机体每档主弹的原始伤害，防止接入时继续沿用项目的 2/3.3 或伤害除 12。
void TestLiveShotDamage() {
    const std::array<std::array<std::vector<int>,8>,4> expected{{
        {{{48},{30,30},{24,30,24},{24,30,24},{24,29,24},{24,28,24},{24,28,24},{23,24,24,23}}},
        {{{48},{32,32},{30,30},{20,28,20},{16,27,16},{16,22,16},{9,17,20,17,9},{9,17,20,17,9}}},
        {{{36},{32},{32},{32},{16,16},{13,13},{8,12,8},{8,12,8}}},
        {{{32},{32},{22,22},{22,22},{20,20},{15,20,15},{15,20,15},{12,15,20,15,12}}},
    }};
    constexpr std::array<int,8> powers{8,16,32,48,64,80,96,128};
    constexpr std::array<int,8> upper{15,31,47,63,79,95,127,128};
    auto pool=std::make_unique<BulletSystem>();
    stage1::Stage1Content content;
    std::size_t channels=0;
    for(const auto& definition:nc::Shots()) {
        assert(definition.levels.size()==9 && definition.levels[0].minimumPower==0);
        for(const auto& level:definition.levels)channels+=level.shots.size();
        assert(&nc::ForPower(definition.id,-1)==&definition.levels.front());
        assert(&nc::ForPower(definition.id,999)==&definition.levels.back());
        for(std::size_t tier=0;tier<powers.size();++tier)for(int power:{powers[tier],upper[tier]})for(bool focus:{false,true}) {
            Player player;player.ConfigureRun(3,3,false,definition.id);player.AdjustPower(power-player.Power());
            const auto& level=nc::ForPower(definition.id,power);
            assert(level.minimumPower==powers[tier] && level.maximumPower==upper[tier]);
            std::vector<int> mainDamage;
            std::vector<bool> observed(level.shots.size());
            for(int frame=0;frame<30;++frame) {
                player.Update(frame==0 ? 0 : 1.0F/60,{.focus=focus,.fire=true},*pool,content.PlayerVisuals(definition.id),true);
                pool->Pool().ForEachActive([&](const Bullet& bullet) {
                    std::size_t index=0;
                    while(index<level.shots.size() && level.shots[index].sourceRva!=bullet.sourceShotRva)++index;
                    assert(index<level.shots.size());observed[index]=true;
                    const auto& raw=level.shots[index];
                    if(raw.kind==3)assert(bullet.damage==0 && bullet.damagePerSecond==raw.damage*60);
                    else assert(bullet.damage==raw.damage && bullet.damagePerSecond==0);
                    if(raw.module==0 && frame==0)mainDamage.push_back(bullet.damage);
                });
                pool->Reset();
            }
            assert(mainDamage==expected[static_cast<int>(definition.id)][tier]);
            for(std::size_t index=0;index<level.shots.size();++index) {
                const auto& raw=level.shots[index];
                assert(observed[index]==(raw.kind==3 || (raw.phase<raw.period && raw.phase<30)));
            }
        }
    }
    assert(channels==193);
    bool invalid=false;
    try { (void)nc::ForPower(static_cast<ShotType>(99),8); } catch(const std::invalid_argument&) { invalid=true; }
    assert(invalid);
}

void TestDamageExtension() {
    DamageContext context{DamageSource::Projectile,DamageTarget::Enemy,ShotType::HomingAmulet,48,true,false,nullptr,0,0,0};
    assert(ResolveDamage(context).amount==48 && ResolveDamage(context).coverage==DamageCoverage::BaseOnly);
    context.target=DamageTarget::Boss;context.spell=true;
    assert(std::abs(ResolveDamage(context).amount-48*0.8F/7)<0.0001F);
    assert(ResolveDamage(context).coverage==DamageCoverage::Fallback);
    context.source=DamageSource::Bomb;
    assert(ResolveDamage(context).amount==16 && ResolveDamage(context).coverage==DamageCoverage::NativeRule);
    context.baseDamage=200;
    assert(ResolveDamage(context).amount==23); // 先截到 70，再按原作整数 /3。
    context.bombUsedDuringSpell=false;
    assert(ResolveDamage(context).amount==0);
    context.bombUsedDuringSpell=true;
    struct Rules final:DamageRuleProvider {
        std::optional<DamageResult> Resolve(const DamageContext& value) const override {
            if(value.source==DamageSource::Bomb)return std::nullopt;
            return DamageResult{12,DamageCoverage::NativeRule};
        }
    } rules;
    assert(ResolveDamage(context,&rules).coverage==DamageCoverage::NativeRule);
    // 提供方可以替换接触后的 HP，但不能在 Bomb 无接触或目标无敌时制造命中。
    struct BombRules final:DamageRuleProvider {
        mutable int calls=0;
        std::optional<DamageResult> Resolve(const DamageContext&) const override {
            ++calls;return DamageResult{9,DamageCoverage::Fallback};
        }
    } bombRules;
    auto bombContext=context;bombContext.baseDamage=0;
    assert(ResolveDamage(bombContext,&bombRules).amount==0 && bombRules.calls==0);
    bombContext.baseDamage=200;bombContext.damageable=false;
    assert(ResolveDamage(bombContext,&bombRules).amount==0 && bombRules.calls==0);
    bombContext.damageable=true;
    const auto overridden=ResolveDamage(bombContext,&bombRules);
    assert(overridden.amount==9 && overridden.coverage==DamageCoverage::Fallback && bombRules.calls==1);
    context.source=DamageSource::Laser;
    assert(ResolveDamage(context,&rules).amount==12 && ResolveDamage(context,&rules).coverage==DamageCoverage::NativeRule);
    context.damageable=false;
    assert(ResolveDamage(context,&rules).amount==0);
    context.baseDamage=std::numeric_limits<float>::quiet_NaN();
    bool invalid=false;
    try { (void)ResolveDamage(context); } catch(const std::invalid_argument&) { invalid=true; }
    assert(invalid);
    DamageContext duringBomb{DamageSource::Projectile,DamageTarget::Enemy,ShotType::HomingAmulet,48,true,false,nullptr,0,0,0,true};
    assert(ResolveDamage(duringBomb).amount==16);
    duringBomb.baseDamage=2;assert(ResolveDamage(duringBomb).amount==1);
    duringBomb.source=DamageSource::Laser;duringBomb.baseDamage=48;duringBomb.contactSeconds=2.0F/60;
    assert(ResolveDamage(duringBomb).amount==16); // 持续弹先恢复每接触帧的原始伤害再 /3，不能按显示帧重复取最低值。
    for(int fps:{30,60,144}) {
        duringBomb.contactSeconds=1.0F/fps;duringBomb.baseDamage=48*60*duringBomb.contactSeconds;
        assert(std::abs(ResolveDamage(duringBomb).amount-16*60*duringBomb.contactSeconds)<0.00001F);
    }
}
} // namespace

/// @brief 运行数据接入检查，不加载原作安装、图形或 Python；失败即断言终止。
void RunNativeCombatTests() { TestCatalogAndHealth();TestLiveShotDamage();TestDamageExtension(); }
