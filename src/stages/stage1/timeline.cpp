// 本文件将新典一面 60 Hz 出生表转换为时间轴；道中战期间时间继续走，到点的杂鱼由 Game 跳过。
#include "stages/stage1/data.hpp"
#include "gameplay/data/native_combat.hpp"
#include "stages/stage1/reference_script.hpp"
#include <vector>
namespace koumakyou::stage1 {
namespace {
const std::vector<StageEvent> kEvents = [] {
    std::vector<StageEvent> events;
    bool midbossAdded=false;
    for (const auto& birth:native_combat::Stage(1)->health) {
        if(birth.scope!=native_combat::HealthScope::TimelineSpawn || birth.targetSub<0 || birth.targetSub>4)continue;
        if(!midbossAdded && birth.frame>1882) {
            events.push_back({.atFrame=1882,.action=StartBoss{},.id=1});
            midbossAdded=true;
        }
        ReferenceEnemyParams params{ReferencePosition({birth.position.x==-999 ? 192 : birth.position.x,birth.position.y,birth.position.z}),
            birth.targetSub,birth.opcode==2 || birth.opcode==6,birth.position.x==-999,static_cast<float>(native_combat::RequireLiteral(&birth)),birth.score,
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
