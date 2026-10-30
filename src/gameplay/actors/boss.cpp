// 本文件实现 Boss 的发弹点、默认血条快照及读取快照颜色的通用画笔；HUD 只提供绘制区域。

#include "gameplay/actors/boss.hpp"

#include "gameplay/bullets/patterns.hpp"
#include <algorithm>

namespace koumakyou {

BossHealthBar Boss::HealthBar() const {
    if (!active || maxHealth <= 0) return {};
    BossHealthBar bar;
    bar.count = 1;
    bar.segments.push_back({1,std::clamp(health/maxHealth,0.0F,1.0F),WHITE});
    return bar;
}

void DrawBossHealthBar(const BossHealthBar& bar,Rectangle area) {
    DrawRectangleRec(area,bar.background);float offset=0;
    for(const auto& segment:bar.segments) {
        if(segment.remaining>0)DrawRectangleRec({area.x+offset*area.width,area.y,segment.remaining*area.width,area.height},segment.color);
        offset+=segment.capacity; // 累加配置容量，不能按剩余长度挪动后续段。
    }
}
void Boss::DrawHealthBar(Rectangle area) const {DrawBossHealthBar(HealthBar(),area);}

Vector3 Boss::MuzzlePosition(Vector3 playerPosition) const {
    return pattern::Add(position,
        pattern::Scale(BulletSystem::DirectionTo(position, playerPosition),
                       hitRadius * 0.55F));
}

}  // namespace koumakyou
