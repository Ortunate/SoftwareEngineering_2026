// 本文件负责验证玩家封装后的移动、射击、Bomb 和敌弹接触规则。

#include "gameplay/actors/player.hpp"
#include "app/math.hpp"
#include "stages/stage1/ids.hpp"

#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>
#include <tuple>
#include <vector>

namespace {
/// @brief 将弹道延伸到距视点 depth 世界单位的准星平面，返回相对视点的落点；forward 须归一化。
Vector3 ShotPlanePoint(const koumakyou::Bullet& bullet, Vector3 position, Vector3 forward, float depth = 20.0F) {
    const auto relative = koumakyou::game_math::Subtract(bullet.position, position);
    const float originDepth = relative.x * forward.x + relative.y * forward.y + relative.z * forward.z;
    const float speed = bullet.velocity.x * forward.x + bullet.velocity.y * forward.y + bullet.velocity.z * forward.z;
    assert(speed > 0);
    return koumakyou::game_math::Add(relative,
        koumakyou::game_math::Scale(bullet.velocity, (depth - originDepth) / speed));
}

/// @brief 返回默认汇聚平面的散射角（弧度）；此处仅用于默认朝向的测试。
float DefaultShotAngle(const koumakyou::Bullet& bullet) {
    const auto point = ShotPlanePoint(bullet, {0, 0, -6.5F}, {0, 0, 1});
    return std::abs(std::atan2(point.x, point.z));
}

// 验证发射点保持在画面下方，散射落点围绕准星对称，cyan 仍保留双模块和后续追踪。
void TestShotSpread() {
    using koumakyou::Bullet;
    using koumakyou::BulletSystem;
    using koumakyou::Player;
    using koumakyou::PlayerInput;
    const auto dot = [](Vector3 a, Vector3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    auto storage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *storage;
    for (float yaw : {0.0F, 1.1F, 3.1F}) {
        for (float pitch : {-1.45F, 0.0F, 1.45F}) {
            for (bool focus : {false, true}) {
                Player player;
                player.AdjustPower(128);
                player.SetView(yaw, pitch);
                bullets.Reset();
                player.Update(0.0F, {.focus = focus, .fire = true}, bullets,
                              koumakyou::stage1::kPlayerGoldCard,
                              koumakyou::stage1::kPlayerCyanCard, true);
                const Vector3 forward = BulletSystem::DirectionFromYawPitch(yaw, pitch);
                const Vector3 right{-std::cos(yaw), 0.0F, std::sin(yaw)};
                const Vector3 up{-std::sin(yaw) * std::sin(pitch), std::cos(pitch),
                                 -std::cos(yaw) * std::sin(pitch)};
                float goldHalfAngle = 0.0F;
                float cyanHalfAngle = 0.0F;
                float goldLateralSum = 0.0F;
                float cyanLateralSum = 0.0F;
                bullets.Pool().ForEachActive([&](const Bullet& bullet) {
                    const bool cyan = bullet.visual == koumakyou::stage1::kPlayerCyanCard;
                    const float speed = cyan ? 20.0F*10/12 : 20.0F;
                    assert(std::abs(BulletSystem::Length(bullet.velocity) - speed) < 0.0001F);
                    const float lateral = dot(bullet.velocity, right);
                    const float forwardSpeed = dot(bullet.velocity, forward);
                    assert(forwardSpeed > 0.0F);
                    const auto point = ShotPlanePoint(bullet, player.Position(), forward);
                    assert(std::abs(dot(point, up)) < 0.0001F);
                    const Vector3 relative{bullet.position.x - player.Position().x,
                                           bullet.position.y - player.Position().y,
                                           bullet.position.z - player.Position().z};
                    assert(std::abs(dot(relative, forward) - 1.2F) < 0.0001F);
                    assert(std::abs(dot(relative, up) + (cyan ? 0.35F : 0.6F)) < 0.0001F);
                    const float originSide = dot(relative, right);
                    if (cyan) {
                        assert(std::abs(std::abs(originSide) - 0.78F) < 0.0001F);
                        assert(originSide * lateral > 0.0F);
                    } else {
                        assert(std::abs(std::abs(originSide)-8*Player::kOptionWorldHeight/36) < 0.0001F);
                    }
                    const float angle = std::abs(std::atan2(dot(point, right), dot(point, forward)));
                    if (cyan) {
                        cyanHalfAngle = std::max(cyanHalfAngle, angle);
                        cyanLateralSum += lateral;
                    } else {
                        goldHalfAngle = std::max(goldHalfAngle, angle);
                        goldLateralSum += lateral;
                    }
                });
                assert(std::abs(goldHalfAngle - 7.0F * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
                assert(std::abs(cyanHalfAngle - 20.0F * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
                assert(std::abs(goldLateralSum) < 0.0001F && std::abs(cyanLateralSum) < 0.0001F);
            }
        }
    }

    // gold 每轮都对称，cyan 每个发射时刻都从左右模块成对生成。
    for (int power : {16, 32, 80, 96}) {
        Player player;
        player.AdjustPower(power - player.Power());
        bullets.Reset();
        for (float deltaTime : {0.0F, 0.09F}) {
            player.Update(deltaTime, {.fire = true}, bullets,
                          koumakyou::stage1::kPlayerGoldCard,
                          koumakyou::stage1::kPlayerCyanCard, true);
            float goldRoundSum = 0.0F;
            bullets.Pool().ForEachActive([&](const Bullet& bullet) {
                if (bullet.visual == koumakyou::stage1::kPlayerGoldCard)
                    goldRoundSum += bullet.velocity.x;
            });
            assert(std::abs(goldRoundSum) < 0.0001F);
        }
        float goldSum = 0.0F;
        float cyanSum = 0.0F;
        bullets.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.visual == koumakyou::stage1::kPlayerGoldCard)
                goldSum += bullet.velocity.x;
            else
                cyanSum += bullet.velocity.x;
        });
        assert(std::abs(goldSum) < 0.0001F && std::abs(cyanSum) < 0.0001F);
    }

    Player player;
    player.AdjustPower(128);
    bullets.Reset();
    player.Update(0.0F, {.fire = true}, bullets,
                  koumakyou::stage1::kPlayerGoldCard, koumakyou::stage1::kPlayerCyanCard, true);
    const std::array<Vector3, 1> target{{{0.0F, 0.0F, 6.0F}}};
    bullets.Update(0.1F, target);
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        if (bullet.visual == koumakyou::stage1::kPlayerCyanCard) {
            assert(bullet.position.x * bullet.velocity.x < 0.0F);
            assert(std::abs(BulletSystem::Length(bullet.velocity) - 20.0F*10/12) < 0.0001F);
        }
    });
}

// 在多个朝向、距离和火力档位验证真实弹道落点；近距离不能越过目标生成，偏离准星的敌人不能吸附弹道。
void TestShotAim() {
    using namespace koumakyou;
    using namespace koumakyou::game_math;
    const auto dot = [](Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; };
    auto bullets = std::make_unique<BulletSystem>();
    for (const auto shot : {ShotType::HomingAmulet, ShotType::Needle}) {
        for (float yaw : {0.0F, 1.1F, 3.1F}) for (float pitch : {-1.45F, 0.0F, 1.45F}) {
            const auto forward = BulletSystem::DirectionFromYawPitch(yaw, pitch);
            const Vector3 right{-std::cos(yaw), 0, std::sin(yaw)};
            const Vector3 up{-std::sin(yaw)*std::sin(pitch), std::cos(pitch), -std::cos(yaw)*std::sin(pitch)};
            for (int power : {8, 16, 32, 48, 64, 80, 96, 128}) for (float depth : {0.3F, 1.2F, 3.0F, 12.0F, 30.0F}) {
                Player player;
                player.ConfigureRun(3, 3, false, shot);
                player.SetView(yaw, pitch);
                player.AdjustPower(power - player.Power());
                // 先提供远敌人，再提供准星内的近敌人和准星外的更近敌人，防止按数组次序或距离吸附。
                const auto point = Add(player.Position(), Scale(forward, depth));
                const std::array<PlayerAimTarget, 4> targets{{
                    {Add(player.Position(), Scale(forward, depth+5)), 0.1F},
                    {Add(point, Scale(right, 0.02F)), 0.1F},
                    {Add(player.Position(), Add(Scale(forward, depth*0.5F), Scale(up, 1))), 0.1F},
                    {Subtract(player.Position(), Scale(forward, 1)), 0.1F},
                }};
                bullets->Reset();
                player.Update(0, {.fire=true}, *bullets, stage1::kPlayerGoldCard, shot == ShotType::Needle ? stage1::kPlayerNeedle : stage1::kPlayerCyanCard, true, targets);
                float goldLateralSum = 0;
                int centerShots = 0;
                bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                    const auto relative = Subtract(bullet.position, player.Position());
                    assert(dot(relative, forward) > 0 && dot(relative, forward) < depth);
                    assert(dot(relative, up) < 0);
                    const auto impact = ShotPlanePoint(bullet, player.Position(), forward, depth);
                    assert(std::abs(dot(impact, up)) < 0.0001F);
                    assert(std::abs(dot(impact, forward)-depth) < 0.0001F);
                    assert(std::abs(BulletSystem::Length(bullet.velocity) -
                        (bullet.homingTurnRate > 0 ? 20.0F*10/12 : bullet.visual == stage1::kPlayerNeedle ? 20.0F*22/12 : 20.0F)) < 0.0001F);
                    if (bullet.visual != stage1::kPlayerGoldCard) return;
                    goldLateralSum += dot(impact, right);
                    if (std::abs(dot(impact, right)) < 0.00001F) {
                        ++centerShots;
                        // 奇数发中央弹确实经过准星指向的球体内部，且瞄准点不随偏心球心移动。
                        assert(DistanceSquared(Add(player.Position(), impact), targets[1].position) < 0.1F*0.1F);
                    }
                });
                assert(std::abs(goldLateralSum) < 0.0001F);
                const int expectedCenters = shot==ShotType::HomingAmulet
                    ? (power==16 ? 0 : power==128 ? 2 : 1)
                    : (power==16 || power==32 ? 0 : 1);
                assert(centerShots == expectedCenters);
            }
        }
    }

    // 相交顺序按球体前表面决定，而不是球心深度；准星没碰到任何目标则回退默认距离。
    for (bool intersects : {false, true}) {
        Player player;
        player.AdjustPower(16 - player.Power());
        const std::array<PlayerAimTarget, 3> targets{{{{0, 0, 3.5F}, intersects ? 6.0F : 0.0F},
                                                    {{0, 0, 1.5F}, intersects ? 0.1F : 0.0F},
                                                    {{2, 0, -1.5F}, 0.1F}}};
        bullets->Reset();
        player.Update(0, {.fire=true}, *bullets, stage1::kPlayerGoldCard, stage1::kPlayerCyanCard, true, targets);
        const float depth = intersects ? 10.0F : 20.0F;
        bullets->Pool().ForEachActive([&](const Bullet& bullet) {
            const auto impact = ShotPlanePoint(bullet, player.Position(), {0, 0, 1}, depth);
            assert(std::abs(impact.y) < 0.0001F);
        });
    }
}

// 逐帧核对新典实际发射时刻和角度，尤其是满火力跨 30 帧窗口时的两帧间隔。
void TestHomingSchedule() {
    using namespace koumakyou;
    using Event = std::pair<int, int>; // 发射帧、单侧散射角（度）；每个事件对应左右各一颗。
    struct Case { int minimum; int maximum; std::vector<Event> events; };
    const std::array<Case, 8> cases{{
        {8, 15, {{0,30}}}, {16, 31, {{0,30}}}, {32, 47, {{0,30}}},
        {48, 63, {{0,30},{15,30}}},
        {64, 79, {{0,30},{0,60},{15,30}}},
        {80, 95, {{0,20},{10,40},{20,60}}},
        {96, 127, {{0,20},{5,40},{10,60},{15,20},{20,40},{25,60}}},
        {128, 128, {{0,20},{4,40},{8,60},{12,80},{16,20},{20,40},{24,60},{28,80}}},
    }};
    auto bullets = std::make_unique<BulletSystem>();
    const auto collect = [&](int frame, std::vector<Event>& events) {
        int count = 0;
        float lateralSum = 0;
        bullets->Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.homingTurnRate <= 0) return;
            ++count;
            lateralSum += bullet.velocity.x;
            assert(std::abs(bullet.position.x) >= 0.26F-0.0001F && std::abs(bullet.position.x) <= 0.78F+0.0001F);
            assert(bullet.position.x*bullet.velocity.x > 0);
            if (bullet.position.x > 0)
                events.emplace_back(frame, static_cast<int>(std::lround(DefaultShotAngle(bullet)*180/std::numbers::pi_v<float>)));
        });
        assert(count % 2 == 0 && std::abs(lateralSum) < 0.0001F);
        bullets->Reset();
    };
    for (const auto& scenario : cases) for (int power : {scenario.minimum, scenario.maximum}) {
        for (bool focus : {false, true}) {
            Player player;
            player.AdjustPower(power - player.Power());
            std::vector<Event> actual;
            for (int frame=0; frame<60; ++frame) {
                player.Update(frame==0 ? 0 : 1.0F/60, {.focus=focus, .fire=true}, *bullets,
                              stage1::kPlayerGoldCard, stage1::kPlayerCyanCard, true);
                collect(frame, actual);
            }
            auto expected = scenario.events;
            for (const auto& [frame, angle] : scenario.events) expected.emplace_back(frame+30, angle);
            assert(actual == expected);
        }
        // 不同帧率和中途切换 Focus 均应在一秒内生成相同的左右成对通道。
        for (int fps : {30, 144}) {
            Player player;
            player.AdjustPower(power - player.Power());
            std::vector<Event> actual;
            for (int frame=0; frame<fps; ++frame) {
                player.Update(frame==0 ? 0 : 1.0F/fps, {.focus=frame%2==0, .fire=true}, *bullets,
                              stage1::kPlayerGoldCard, stage1::kPlayerCyanCard, true);
                collect(0, actual);
            }
            std::vector<Event> expected;
            for (int cycle=0; cycle<2; ++cycle)
                for (const auto& event : scenario.events) expected.emplace_back(0, event.second);
            assert(actual == expected);
        }
    }
    Player player;
    player.AdjustPower(96 - player.Power());
    std::vector<Event> burst;
    player.Update(0, {.fire=true}, *bullets, 0, 1, true);
    collect(0, burst);
    player.Update(0.5F, {}, *bullets, 0, 1, true);
    collect(0, burst);
    assert(burst.size() == 6); // 点射仍完成当前 30 帧窗口，但松键不会启动第二轮。
    player.Update(1, {}, *bullets, 0, 1, true);
    assert(bullets->Pool().ActiveCount() == 0);
    player.Update(0, {.fire=true}, *bullets, 0, 1, true);
    bullets->Reset();
    player.Update(0.5F, {.fire=true}, *bullets, 0, 1, false);
    assert(bullets->Pool().ActiveCount() == 0);
    player.Update(0, {.fire=true}, *bullets, 0, 1, true);
    std::vector<Event> restarted;
    collect(0, restarted);
    assert((restarted == std::vector<Event>{{0,20}}));

    // Power 改变只切换通道表，不能重启周期造成升级时额外齐射。
    player.Reset();
    player.Update(0, {.fire=true}, *bullets, 0, 1, true);
    bullets->Reset();
    player.Update(4.0F/60, {.fire=true}, *bullets, 0, 1, true);
    bullets->Reset();
    player.AdjustPower(88);
    player.Update(1.0F/60, {.fire=true}, *bullets, 0, 1, true);
    std::vector<Event> upgraded;
    collect(5, upgraded);
    assert((upgraded == std::vector<Event>{{5,40}}));
    player.ResetPose();
    player.Update(0, {.fire=true}, *bullets, 0, 1, true);
    std::vector<Event> respawned;
    collect(0, respawned);
    assert((respawned == std::vector<Event>{{0,20}}));
}

// 太极圈与副弹共用世界坐标；验证收拢、反向切换、帧率差异及近距离缩放不产生发射点偏移。
void TestPlayerOptions() {
    using namespace koumakyou;
    using namespace koumakyou::game_math;
    auto bullets = std::make_unique<BulletSystem>();
    Player visibility;
    assert(visibility.Power() == Player::kMinPower && visibility.OptionsVisible());
    visibility.AdjustPower(-999);
    assert(visibility.Power() == Player::kMinPower && visibility.OptionsVisible());
    visibility.AdjustPower(12);
    assert(visibility.TakeHit());
    assert(visibility.Power() == Player::kMinPower && visibility.OptionsVisible());
    for (float yaw : {0.0F,1.1F,3.1F}) for (float pitch : {-1.45F,0.0F,1.45F}) {
        Player player;
        player.AdjustPower(128);
        player.SetView(yaw,pitch);
        const auto forward=BulletSystem::DirectionFromYawPitch(yaw,pitch);
        const Vector3 right{-std::cos(yaw),0,std::sin(yaw)};
        const Vector3 up{-std::sin(yaw)*std::sin(pitch),std::cos(pitch),-std::cos(yaw)*std::sin(pitch)};
        const auto dot=[](Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
        for (int frame=0;frame<=16;++frame) {
            const bool focus=frame<=8;
            player.Update(frame==0 ? 0 : 1.0F/60,{.focus=focus,.fire=true},*bullets,
                          stage1::kPlayerGoldCard,stage1::kPlayerCyanCard,true);
            const auto positions=player.OptionPositions();
            const float separation=BulletSystem::Length(Subtract(positions[1],positions[0]));
            if (frame==0 || frame==16)assert(std::abs(separation-1.56F)<0.0001F);
            if (frame==8)assert(std::abs(separation-0.52F)<0.0001F);
            for (int side=0;side<2;++side) {
                const auto relative=Subtract(positions[side],player.Position());
                assert(std::abs(dot(relative,forward)-1.2F)<0.0001F);
                assert(std::abs(dot(relative,up)+0.35F)<0.0001F);
                assert(dot(relative,right)*(side==0 ? -1 : 1)>0);
            }
            bullets->Pool().ForEachActive([&](const Bullet& bullet){
                if(bullet.homingTurnRate<=0)return;
                assert(std::min(DistanceSquared(bullet.position,positions[0]),
                                DistanceSquared(bullet.position,positions[1]))<0.000001F);
            });
            bullets->Reset();
        }
        assert(std::abs(player.VisualAge()-16.0/60)<0.0001);
        const auto before=player.OptionPositions();
        player.Update(0,{.focus=true},*bullets,0,1,false);
        assert(DistanceSquared(before[0],player.OptionPositions()[0])<0.000001F);
        player.ResetPose();
        assert(player.VisualAge()==0 && player.OptionScale()==1);
    }
    float expectedSeparation=0;
    for (int steps : {3,6,14}) {
        Player player;
        for(int step=0;step<steps;++step)player.Update(0.1F/steps,{.focus=true},*bullets,0,1,false);
        const auto positions=player.OptionPositions();
        const float separation=BulletSystem::Length(Subtract(positions[1],positions[0]));
        if(expectedSeparation==0)expectedSeparation=separation;
        assert(std::abs(separation-expectedSeparation)<0.0001F);
        player.Update(0.05F,{.focus=false},*bullets,0,1,false);
        const auto wider=player.OptionPositions();
        assert(BulletSystem::Length(Subtract(wider[1],wider[0]))>separation);
    }
    Player close;
    const std::array<PlayerAimTarget,1> target{{{Add(close.Position(),{0,0,0.3F}),0.1F}}};
    close.Update(0,{.fire=true},*bullets,0,1,true,target);
    assert(std::abs(close.OptionScale()-0.125F)<0.0001F);
    const auto positions=close.OptionPositions();
    bullets->Pool().ForEachActive([&](const Bullet& bullet){
        if(bullet.homingTurnRate<=0)return;
        assert(std::min(DistanceSquared(bullet.position,positions[0]),DistanceSquared(bullet.position,positions[1]))<0.000001F);
    });
}
// 使用原始表的明确帧序核对直射针，不能把多束周期合并成主弹冷却，也不能随 Focus 重启相位。
void TestNeedleSchedule() {
    using namespace koumakyou;
    using namespace koumakyou::game_math;
    using Event = std::tuple<int,int,int>; // 窗口帧、横移逻辑像素、新典伤害；每个事件左右各一颗。
    struct Channel { int offset, damage; std::vector<int> frames; };
    struct Case { int minimum, maximum; std::vector<Channel> channels; };
    const std::array<Case,8> cases{{
        {8,15,{{0,12,{0,15}}}}, {16,31,{{0,12,{0,10,20}}}},
        {32,47,{{0,12,{0,8,16,24}}}}, {48,63,{{0,12,{0,8,16,24}}}},
        {64,79,{{8,12,{0,5,10,15,20,25}},{-8,10,{0,8,16,24}}}},
        {80,95,{{8,10,{0,3,6,9,12,15,18,21,24,27}},{-8,10,{0,5,10,15,20,25}}}},
        {96,127,{{12,10,{0,3,6,9,12,15,18,21,24,27}},{-12,10,{0,5,10,15,20,25}},{0,10,{0,10,20}}}},
        {128,128,{{12,10,{0,3,6,9,12,15,18,21,24,27}},{-12,10,{0,3,6,9,12,15,18,21,24,27}},{0,10,{0,5,10,15,20,25}}}},
    }};
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& scenario:cases)for(int power:{scenario.minimum,scenario.maximum})for(bool focus:{false,true}) {
        std::vector<Event> expected;
        for(int cycle=0;cycle<2;++cycle)for(const auto& channel:scenario.channels)
            for(int frame:channel.frames)expected.emplace_back(cycle*30+frame,channel.offset,channel.damage);
        std::stable_sort(expected.begin(),expected.end(),[](const Event& a,const Event& b){return std::get<0>(a)<std::get<0>(b);});
        for(int fps:{30,60,144}) {
            Player player;player.ConfigureRun(3,3,false,ShotType::Needle);player.AdjustPower(power-player.Power());
            std::vector<Event> actual;
            int count=0;
            for(int frame=0;frame<fps;++frame) {
                player.Update(frame==0 ? 0 : 1.0F/fps,{.focus=focus,.fire=true},*bullets,
                              stage1::kPlayerGoldCard,stage1::kPlayerNeedle,true);
                const auto centers=player.OptionPositions();
                const float unit=Player::kOptionWorldHeight/36*player.OptionScale();
                int leftCount=0,rightCount=0;
                bullets->Pool().ForEachActive([&](const Bullet& bullet){
                    if(bullet.visual!=stage1::kPlayerNeedle)return;
                    ++count;
                    assert(bullet.homingTurnRate==0 && bullet.color.a==Player::kShotAlpha);
                    assert(std::abs(BulletSystem::Length(bullet.velocity)-20.0F*22/12)<0.0001F);
                    const int side=DistanceSquared(bullet.position,centers[0])<DistanceSquared(bullet.position,centers[1]) ? 0 : 1;
                    assert(std::abs(bullet.position.y-centers[side].y-16*unit)<0.0001F);
                    assert(std::abs(bullet.position.z-centers[side].z)<0.0001F);
                    const int offset=std::lround((centers[side].x-bullet.position.x)/unit);
                    const int damage=std::lround(bullet.damage);
                    if(side==0) {
                        ++leftCount;
                        actual.emplace_back(fps==60 ? frame : 0,offset,damage);
                    } else ++rightCount;
                });
                assert(leftCount==rightCount);
                bullets->Reset();
            }
            auto timedExpected=expected;
            if(fps!=60)for(auto& event:timedExpected)std::get<0>(event)=0;
            assert(actual==timedExpected && count==static_cast<int>(expected.size()*2));
        }
    }


    Player player;player.ConfigureRun(3,3,false,ShotType::Needle);player.AdjustPower(128);
    player.Update(0,{.fire=true},*bullets,0,1,true);
    bullets->Reset();
    player.Update(0.5F,{},*bullets,0,1,true);
    int remainingNeedles=0,remainingMain=0;
    bullets->Pool().ForEachActive([&](const Bullet& bullet){bullet.visual==1 ? ++remainingNeedles : ++remainingMain;});
    assert(remainingNeedles==46 && remainingMain==25); // 尾窗保留 46 颗针及后续五轮、每轮五颗的主弹。
    const std::array<Vector3,1> target{{{8,0,4}}};
    std::vector<Vector3> before;
    bullets->Pool().ForEachActive([&](const Bullet& bullet){before.push_back(bullet.velocity);});
    bullets->Update(0.1F,target);
    std::size_t index=0;
    bullets->Pool().ForEachActive([&](const Bullet& bullet){assert(DistanceSquared(bullet.velocity,before[index++])<0.000001F);});
    bullets->Reset();
    player.Update(1,{},*bullets,0,1,true);assert(bullets->Pool().ActiveCount()==0);
    player.Update(0,{.fire=true},*bullets,0,1,true);bullets->Reset();
    player.Update(0.1F,{.fire=true},*bullets,0,1,false);assert(bullets->Pool().ActiveCount()==0);
    player.ResetPose();player.Update(0,{.fire=true},*bullets,0,1,true);
    int sides=0;bullets->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.visual==1)++sides;});
    assert(sides==6);bullets->Reset();

    // 从 8 升到 128 时沿用窗口第 4 帧；下一帧只发中心 5 帧通道，不能重新齐射三束。
    player.Reset();player.ConfigureRun(3,3,false,ShotType::Needle);
    player.Update(0,{.fire=true},*bullets,0,1,true);bullets->Reset();
    player.Update(4.0F/60,{.fire=true},*bullets,0,1,true);bullets->Reset();
    player.AdjustPower(120);player.Update(1.0F/60,{.focus=true,.fire=true},*bullets,0,1,true);
    sides=0;bullets->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.visual==1)++sides;});
    assert(sides==2);
}
}  // namespace

/// @brief 运行玩家行为回归用例；断言失败时终止进程。
void RunPlayerTests() {
    TestShotSpread();
    TestShotAim();
    TestHomingSchedule();
    TestPlayerOptions();
    TestNeedleSchedule();
    // 挑战模式反复受击仍可继续；标准模式按额外残机数耗尽后失败。
    koumakyou::Player challenge;
    challenge.ConfigureRun(2, 0, true, koumakyou::ShotType::Needle);
    for (int miss = 0; miss < 12; ++miss) assert(challenge.TakeHit());
    assert(challenge.RemainingLives() == 3);
    assert(challenge.Power() == koumakyou::Player::kMinPower);
    koumakyou::Player standard;
    standard.ConfigureRun(1, 0, false, koumakyou::ShotType::Needle);
    assert(standard.Bombs() == 0 && standard.TakeHit());
    assert(!standard.TakeHit());
    auto needlePool = std::make_unique<koumakyou::BulletSystem>();
    challenge.AdjustPower(128);
    challenge.Update(0, {.fire = true}, *needlePool, koumakyou::stage1::kPlayerGoldCard, koumakyou::stage1::kPlayerNeedle, true);
    assert(needlePool->Pool().ActiveCount() == 11);
    needlePool->Pool().ForEachActive([](const koumakyou::Bullet& shot) {
        assert(shot.homingTurnRate == 0);
        if (shot.visual == koumakyou::stage1::kPlayerGoldCard) assert(shot.damage == 9 || shot.damage == 17 || shot.damage == 20);
        else assert(shot.visual == koumakyou::stage1::kPlayerNeedle && shot.damage==10);
    });
    using koumakyou::Bullet;
    using koumakyou::BulletOwner;
    using koumakyou::BulletSystem;
    using koumakyou::Player;
    using koumakyou::PlayerContact;
    using koumakyou::PlayerInput;

    Player player;
    // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
    auto bulletsStorage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *bulletsStorage;
    assert(player.RemainingLives() == 4);
    assert(player.Bombs() == 3);
    assert(player.Power() == Player::kMinPower);

    Camera3D camera{};
    player.SyncCamera(camera);
    assert(std::abs(camera.position.z + 6.5F) < 0.0001F);
    assert(std::abs(camera.target.z + 5.5F) < 0.0001F);

    // 相同鼠标位移按倍率改变视角；重置玩家后继续使用输入倍率，不影响火力系统。
    for (float scale : {0.1F, 1.0F, 3.0F}) {
        player.Reset();
        PlayerInput look{};
        look.mouseDelta = {10.0F, 0.0F};
        look.mouseSensitivityScale = scale;
        player.Update(1.0F / 60.0F, look, bullets, koumakyou::stage1::kPlayerGoldCard,
                      koumakyou::stage1::kPlayerCyanCard, false);
        player.SyncCamera(camera);
        const float yaw = std::atan2(camera.target.x - camera.position.x,
                                     camera.target.z - camera.position.z);
        assert(std::abs(yaw + 10.0F * 0.0024F * scale) < 0.0001F);
    }
    player.Reset();

    PlayerInput movement{};
    movement.forward = true;
    movement.moveRight = true;
    player.Update(1.0F, movement, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, true);
    const Vector3 moved = player.Position();
    const float displacement = std::sqrt(moved.x * moved.x +
                                         (moved.z + 6.5F) * (moved.z + 6.5F));
    assert(std::abs(displacement - 6.5F) < 0.0001F);

    // 六个方向分别越界，验证 Z 新增的可移动空间及所有表面的内侧留白。
    const std::array<PlayerInput, 6> boundaryInputs{{
        {.forward = true}, {.backward = true}, {.moveLeft = true},
        {.moveRight = true}, {.ascend = true}, {.descend = true},
    }};
    const std::array<Vector3, 6> expectedPositions{{
        {0.0F, 0.0F, 11.15F}, {0.0F, 0.0F, -11.15F},
        {9.65F, 0.0F, -6.5F}, {-9.65F, 0.0F, -6.5F},
        {0.0F, 9.65F, -6.5F}, {0.0F, -9.65F, -6.5F},
    }};
    for (std::size_t index = 0; index < boundaryInputs.size(); ++index) {
        player.Reset();
        player.Update(10.0F, boundaryInputs[index], bullets,
                      koumakyou::stage1::kPlayerGoldCard,
                      koumakyou::stage1::kPlayerCyanCard, false);
        const Vector3 actual = player.Position();
        const Vector3 expected = expectedPositions[index];
        assert(std::abs(actual.x - expected.x) < 0.0001F);
        assert(std::abs(actual.y - expected.y) < 0.0001F);
        assert(std::abs(actual.z - expected.z) < 0.0001F);
        player.SyncCamera(camera);
        assert(camera.position.z == actual.z);
    }

    player.Reset();
    PlayerInput firing{};
    firing.fire = true;
    player.Update(0.1F, firing, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, false);
    assert(bullets.Pool().ActiveCount() == 0);
    player.Update(0.0F, firing, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 3);
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        const bool cyan = bullet.visual == koumakyou::stage1::kPlayerCyanCard;
        assert(bullet.owner == BulletOwner::Player);
        assert(cyan || bullet.visual == koumakyou::stage1::kPlayerGoldCard);
        assert(bullet.color.a == 51);
        assert(bullet.position.y == (cyan ? -0.35F : -0.6F));
        assert(cyan ? std::abs(std::abs(bullet.position.x) - 0.78F) < 0.0001F
                    : bullet.position.x == 0.0F);
        assert(std::abs(bullet.position.z + 5.3F) < 0.0001F);
        assert(cyan ? bullet.velocity.x != 0.0F : std::abs(bullet.velocity.x)<0.00001F);
        const float angle = DefaultShotAngle(bullet);
        assert(std::abs(angle - (cyan ? 30.0F : 0.0F) * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
    });
    player.Update(0.01F, firing, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 3);
    player.Update(0.09F, firing, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 4);

    // 升满只报告一次；继续收集不重复报告，降级后再次达到上限可重新报告。
    assert(player.AdjustPower(999));
    assert(player.Power() == Player::kMaxPower);
    assert(!player.AdjustPower(1));
    assert(!player.AdjustPower(0));
    assert(!player.AdjustPower(-999));
    assert(player.Power() == Player::kMinPower);
    assert(!player.AdjustPower(Player::kMaxPower - Player::kMinPower - 1));
    assert(player.AdjustPower(1));
    assert(!player.AdjustPower(8));
    assert(!player.AdjustPower(-Player::kMaxPower));

    PlayerInput bombInput{};
    bombInput.bomb = true;
    assert(player.Update(0.0F, bombInput, bullets,
                         koumakyou::stage1::kPlayerGoldCard,
                         koumakyou::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 2);
    assert(!player.Update(0.5F, bombInput, bullets,
                          koumakyou::stage1::kPlayerGoldCard,
                          koumakyou::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 2);
    assert(player.Update(4.5F, bombInput, bullets,
                         koumakyou::stage1::kPlayerGoldCard,
                         koumakyou::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 1);

    player.Update(6.0F, {}, bullets, koumakyou::stage1::kPlayerGoldCard,
                  koumakyou::stage1::kPlayerCyanCard, true);
    Bullet graze{};
    graze.active = true;
    graze.position = {0.5F, 0.0F, -6.5F};
    assert(player.ContactEnemyBullet(graze) == PlayerContact::Graze);
    assert(graze.grazed);
    assert(player.ContactEnemyBullet(graze) == PlayerContact::None);

    Bullet hit{};
    hit.active = true;
    hit.position = player.Position();
    assert(player.ContactEnemyBullet(hit) == PlayerContact::Hit);
    assert(!hit.active);
    assert(player.TakeHit());
    assert(player.RemainingLives() == 3);
    assert(player.Bombs() == 2);
    assert(player.Power() == Player::kMinPower);
    Bullet protectedHit{};
    protectedHit.active = true;
    protectedHit.position = player.Position();
    assert(player.ContactEnemyBullet(protectedHit) == PlayerContact::None);
    assert(!protectedHit.active);

    player.AdjustPower(128);
    assert(player.TakeHit());
    assert(player.Power() == 112);
    assert(player.AdjustPower(16));

    // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
    auto trackingStorage = std::make_unique<BulletSystem>();
    BulletSystem& tracking = *trackingStorage;
    Bullet* guided = tracking.Spawn(BulletOwner::Player, {},
        {0.0F, 0.0F, 10.0F}, 0.1F, WHITE,
        koumakyou::stage1::kPlayerCyanCard);
    assert(guided != nullptr);
    guided->homingTurnRate = 4.0F;
    const std::array<Vector3, 1> target{{{8.0F, 0.0F, 8.0F}}};
    tracking.Update(0.2F, target);
    assert(guided->velocity.x > 0.0F);
    assert(std::abs(BulletSystem::Length(guided->velocity) - 10.0F) < 0.0001F);

    player.Reset();
    assert(player.RemainingLives() == 4);
    assert(player.Bombs() == 3);
    assert(player.Power() == Player::kMinPower);
    assert(player.TakeHit());
    assert(player.TakeHit());
    assert(player.TakeHit());
    assert(!player.TakeHit());
    assert(player.RemainingLives() == 0);
}
