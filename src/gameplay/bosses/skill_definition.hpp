// 本文件提供角色内部技能定义的构造辅助，保留原生命寄存器与技能实际 HP 的明确适配边界。
#pragma once
#include "gameplay/bosses/config.hpp"
namespace koumakyou::bosses {
/// @brief nativeHealth 为原生命寄存器初值，reserve 为完成阈值；实际技能 HP 为二者之差。
/// 时限为 60 Hz 内容帧，生命原点偏移只用于原行为兼容，不作为后继技能的血量来源。
inline SkillDefinition CombatSkill(Attack behavior,float nativeHealth,int timeout,float reserve=0) {
    SkillDefinition result;result.behavior=std::move(behavior);result.health=nativeHealth-reserve;
    result.reserveHealth=reserve;result.timeoutFrames=timeout;return result;
}
/// @brief 绑定角色静态符卡信息及宣告延迟，revealFrame 为 60 Hz 内容帧。
inline SkillDefinition SpellSkill(SkillDefinition skill,const char* name,int card,int revealFrame=0) {
    skill.spellName=name;skill.spellCard=card;skill.spellRevealFrame=revealFrame;return skill;
}
}
