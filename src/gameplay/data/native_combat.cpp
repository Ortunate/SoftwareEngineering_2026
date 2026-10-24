// 本文件提供原生战斗数据的静态存储与严格查询；不解包原作文件、不依赖安装路径或 Python。
#include "gameplay/data/native_combat.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace koumakyou::native_combat {
namespace {
#include "native_combat_data.inc"
}
std::span<const StageData> Stages() { return kStages; }
std::span<const ShotDefinition> Shots() { return kShots; }
const StageData* Stage(int stage) { return stage>=1 && stage<=7 ? &kStages[stage-1] : nullptr; }
const ShotLevel& ForPower(ShotType shot,int power) {
    const int index=static_cast<int>(shot);
    if(index<0 || index>=4)throw std::invalid_argument("Invalid native shot");
    power=std::clamp(power,0,128);
    for(const auto& level:kShots[index].levels)
        if(power>=level.minimumPower && power<=level.maximumPower)return level;
    throw std::logic_error("Missing native power tier");
}
const HealthRecord* Instruction(int stage,int sub,int normalIndex) {
    if(normalIndex<0)return nullptr;
    const auto* data=Stage(stage);if(!data)return nullptr;
    for(const auto& record:data->health)
        if(record.scope==HealthScope::Instruction && record.sub==sub && record.normalIndex==normalIndex)return &record;
    return nullptr;
}
const HealthRecord* Setting(int stage,int sub,int opcode,int frame,Difficulty difficulty) {
    const auto* data=Stage(stage);if(!data)return nullptr;
    const HealthRecord* found=nullptr;
    for(const auto& record:data->health)
        if(record.scope==HealthScope::Instruction && record.sub==sub && record.opcode==opcode && record.frame==frame &&
           (record.rankMask&static_cast<std::uint16_t>(difficulty))) {
            if(found)return nullptr;
            found=&record;
        }
    return found;
}
const HealthRecord* Spawn(int stage,int timeline,int frame,int targetSub) {
    const auto* data=Stage(stage);if(!data)return nullptr;
    const HealthRecord* found=nullptr;
    for(const auto& record:data->health)
        if(record.scope==HealthScope::TimelineSpawn && record.timeline==timeline && record.frame==frame && record.targetSub==targetSub) {
            if(found)return nullptr;
            found=&record;
        }
    return found;
}
int RequireLiteral(const HealthRecord* record) {
    if(!record || !record->value || record->variable)throw std::logic_error("Native health value is not a verified literal");
    return *record->value;
}
std::string_view ExecutableHash() { return kExecutableHash; }
} // namespace koumakyou::native_combat
