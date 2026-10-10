// 本文件将新典一面 60 Hz 出生表转换为时间轴；道中战期间时间继续走，到点的杂鱼由 Game 跳过。
#include "stages/stage1/data.hpp"
#include "stages/stage1/reference_births.hpp"
#include "stages/stage1/reference_script.hpp"
#include <vector>
namespace koumakyou::stage1 {
namespace {
const std::vector<StageEvent> kEvents = [] {
    std::vector<StageEvent> events;
    bool midbossAdded=false;
    for (const auto& birth:reference::kBirths) {
        if(!midbossAdded && birth.frame>1882) {
            events.push_back({.atFrame=1882,.action=StartBoss{},.id=1});
            midbossAdded=true;
        }
        ReferenceEnemyParams params{ReferencePosition({birth.x==-999 ? 192 : birth.x,-32,0}),
            birth.sub,birth.mirrored,birth.x==-999,birth.health,birth.score,
            static_cast<unsigned>(birth.frame*31+events.size()),birth.item};
        events.push_back({.atFrame=birth.frame,.action=SpawnEnemies{params,1,0}});
    }
    events.push_back({.atFrame=5092,.action=StartDialogue{},.id=2});
    events.push_back({.action=StartBoss{BossEncounter::Final},.id=3,.afterEndOf=2});
    events.push_back({.action=StartDialogue{true},.id=4,.afterEndOf=3});
    return events;
}();
}
std::span<const StageEvent> Stage1Timeline() { return kEvents; }
} // namespace koumakyou::stage1
