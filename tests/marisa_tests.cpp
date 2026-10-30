// 本文件验证魔理沙两种御札的新典火力分档、主副弹时序、导弹爆破及独立激光槽，不访问图形上下文。
#include "gameplay/actors/player.hpp"
#include "stages/stage1/content.hpp"
#include "app/math.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

namespace {
using namespace koumakyou;

// 数量直接来自新典 0～29 帧窗口；不可达相位不应通过取模改写后额外生成导弹。
void TestNativeSchedules() {
    constexpr std::array<int,8> powers{8,16,32,48,64,80,96,128};
    constexpr std::array<int,8> upper{15,31,47,63,79,95,127,128};
    constexpr std::array<int,8> missileMain{1,1,1,1,2,2,3,3},laserMain{1,1,2,2,2,3,3,5};
    constexpr std::array<int,8> missilePerSecond{4,8,8,8,20,24,32,36};
    stage1::Stage1Content content;
    auto pool=std::make_unique<BulletSystem>();
    for(auto type:{ShotType::MagicMissile,ShotType::IllusionLaser})for(std::size_t level=0;level<powers.size();++level)
        for(int power:{powers[level],upper[level]})for(int fps:{30,60,144})for(bool focus:{false,true}) {
            Player player;player.ConfigureRun(3,3,false,type);player.AdjustPower(power-player.Power());
            const auto visuals=content.PlayerVisuals(type);
            int main=0,missiles=0,lasers=0;
            for(int frame=0;frame<fps;++frame) {
                player.Update(frame==0?0:1.0F/fps,{.focus=focus,.fire=true},*pool,visuals,true);
                pool->Pool().ForEachActive([&](const Bullet& bullet) {
                    assert(bullet.color.a==Player::kShotAlpha && bullet.homingTurnRate==0);
                    if(bullet.playerShotKind!=PlayerShotKind::Laser)assert(std::abs(bullet.radius-6*(20.0F/448))<1e-6F);
                    if(bullet.playerShotKind==PlayerShotKind::Missile) {
                        ++missiles;assert(bullet.impactVisual>=stage1::kMarisaExplosion && bullet.impactVisual<stage1::kMarisaExplosion+4);
                        assert(std::abs(bullet.acceleration.z-30)<0.0001F);
                    } else if(bullet.playerShotKind==PlayerShotKind::Laser) {
                        ++lasers;assert(std::abs(bullet.beamLength-480*(20.0F/448))<0.00001F && bullet.damagePerSecond>0);
                    } else {
                        ++main;assert(bullet.visual==stage1::kMarisaPrimary);
                        assert(std::abs(BulletSystem::Length(bullet.Velocity())-20)<0.0001F);
                    }
                });
                pool->Reset();
            }
            const bool laser=type==ShotType::IllusionLaser;
            assert(main==12*(laser?laserMain[level]:missileMain[level]));
            assert(missiles==(laser?0:missilePerSecond[level]));
            assert(lasers==(laser?2:0));
        }
}

void TestMissileMotionAndImpact() {
    stage1::Stage1Content content;const auto visuals=content.PlayerVisuals(ShotType::MagicMissile);
    auto pool=std::make_unique<BulletSystem>();
    Vector3 expected{};
    for(int fps:{30,60,144}) {
        Player player;player.ConfigureRun(3,3,false,ShotType::MagicMissile);
        player.Update(0,{.fire=true},*pool,visuals,true);
        Bullet* missile=nullptr;
        pool->Pool().ForEachActive([&](Bullet& bullet){if(bullet.playerShotKind==PlayerShotKind::Missile)missile=&bullet;else bullet.active=false;});
        assert(missile);const auto start=missile->position,velocity=missile->Velocity();
        for(int frame=0;frame<fps/2;++frame)pool->Update(1.0F/fps);
        const auto moved=game_math::Subtract(missile->position,start);
        assert(std::abs(moved.z-(velocity.z*0.5F+0.5F*30*0.5F*(0.5F+1.0F/60)))<0.0001F);
        if(fps==30)expected=missile->position;
        else assert(game_math::DistanceSquared(expected,missile->position)<0.000001F);
        pool->ExplodePlayerMissile(*missile);
        assert(missile->active && missile->playerShotKind==PlayerShotKind::Explosion);
        assert(missile->visual==stage1::kMarisaExplosion && std::abs(missile->radius-32*Player::kOptionWorldHeight/16*0.5F)<0.0001F);
        assert(missile->age==0 && std::abs(missile->maxAge-20.0F/60)<0.0001F);
        pool->Update(0.1F);
        assert(std::abs(missile->ContactDamage()-24)<0.0001F); // 原始 18 取四分之一为 4，每接触帧 4 点，即每秒 240 原始点。
        pool->Update(0.24F);assert(missile->pendingExpiration);
        pool->Update(0);assert(!missile->active);
        pool->Reset();
    }
    // 生命周期末尾跨帧也只结算有效部分；20/60 秒的总爆破伤害在所有帧率下完全一致。
    for(int fps:{30,60,144}) {
        auto* burst=pool->Spawn(BulletOwner::Player,{}, {},0.5F,WHITE,stage1::kMarisaExplosion);
        burst->playerShotKind=PlayerShotKind::Explosion;burst->damage=0;burst->damagePerSecond=20;burst->maxAge=20.0F/60;
        float damage=0;
        for(int frame=0;frame<fps;++frame) {
            pool->Update(1.0F/fps);
            pool->Pool().ForEachActive([&](const Bullet& bullet){damage+=bullet.ContactDamage();});
        }
        assert(std::abs(damage-20.0F/3)<0.0001F);pool->Reset();
    }
}

void TestAnchoredLasers() {
    stage1::Stage1Content content;auto pool=std::make_unique<BulletSystem>();
    Player player;player.ConfigureRun(3,3,false,ShotType::IllusionLaser);
    const auto visuals=content.PlayerVisuals(player.Shot());
    player.Update(0,{.fire=true},*pool,visuals,true);
    int count=0;
    pool->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.playerShotKind==PlayerShotKind::Laser) {
        ++count;assert(std::abs(bullet.maxAge-80.0F/60)<0.0001F);assert(bullet.playerSlot>=0 && bullet.playerSlot<2);
        assert(std::abs(bullet.laserFadeAt-50.0F/60)<0.0001F);
    }});
    assert(count==2);
    for(int frame=0;frame<20;++frame) {
        player.Update(1.0F/60,{.focus=true,.fire=true},*pool,visuals,true);pool->Update(1.0F/60);
    }
    player.SetView(0.7F,0.3F);
    player.Update(0,{.fire=true},*pool,visuals,true);
    count=0;
    pool->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.playerShotKind==PlayerShotKind::Laser) {
        ++count;assert(game_math::DistanceSquared(bullet.position,player.OptionPositions()[bullet.playerSlot])<0.000001F);
        assert(bullet.Direction().x>0 && bullet.Direction().y>0);
    }});
    assert(count==2); // 多帧按住射击不能堆叠更多光束。
    player.Update(0,{},*pool,visuals,false);
    pool->Pool().ForEachActive([](const Bullet& bullet){assert(bullet.playerShotKind!=PlayerShotKind::Laser);});
    pool->Reset();player.ResetPose();player.Update(0,{.fire=true},*pool,visuals,true);
    player.ResetPose();player.Update(0,{},*pool,visuals,true);
    pool->Pool().ForEachActive([](const Bullet& bullet){assert(bullet.playerShotKind!=PlayerShotKind::Laser);});

    // 原始角色速度 5/2.5 相对灵梦 4/2，三维速度保持相同的 1.25 倍关系。
    pool->Reset();
    for(bool focus:{false,true}) {
        Player reimu,marisa;marisa.ConfigureRun(3,3,false,ShotType::MagicMissile);
        reimu.Update(0.2F,{.forward=true,.focus=focus},*pool,0,1,false);
        marisa.Update(0.2F,{.forward=true,.focus=focus},*pool,visuals,false);
        assert(std::abs((marisa.Position().z+6.5F)-(reimu.Position().z+6.5F)*1.25F)<0.0001F);
    }
}

// 冷却从真实逻辑发射时刻计算，170 帧不必对齐 30 帧窗口或显示帧；持续按住时到期才重发。
void TestLaserCooldowns() {
    stage1::Stage1Content content;auto pool=std::make_unique<BulletSystem>();
    for(int power:{8,16})for(int fps:{30,60,144}) {
        Player player;player.ConfigureRun(3,3,false,ShotType::IllusionLaser);player.AdjustPower(power-player.Power());
        const int cooldown=power==8?120:170;
        const int displayFrame=static_cast<int>(std::ceil(cooldown*fps/60.0));
        std::vector<int> born;
        for(int frame=0;frame<=displayFrame;++frame) {
            player.Update(frame==0?0:1.0F/fps,{.fire=true},*pool,content.PlayerVisuals(player.Shot()),true);
            pool->Pool().ForEachActive([&](Bullet& bullet){
                if(bullet.playerShotKind==PlayerShotKind::Laser && !bullet.grazed) {
                    born.push_back(frame);bullet.grazed=true; // 自机不参与擦弹；借此标记独立发射，不检查内部冷却变量。
                }
            });
            pool->Update(frame==0?0:1.0F/fps);
        }
        assert((born==std::vector<int>{0,0,displayFrame,displayFrame}));pool->Reset();
    }
}
} // namespace

/// @brief 运行魔理沙机体回归检查；任一原始时序、持续弹约定或素材编号不符即断言终止。
void RunMarisaTests() {
    TestNativeSchedules();TestMissileMotionAndImpact();TestAnchoredLasers();TestLaserCooldowns();
}
