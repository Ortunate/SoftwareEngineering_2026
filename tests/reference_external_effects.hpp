// 本文件核对102附着、118与原生6环境请求；只在参考侧执行源坐标/ARGB到世界坐标/RGBA转换。
#pragma once
#include "stage_extra_flandre_reference.hpp"
#include "gameplay/actors/behavior.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace koumakyou::stage_extra::reference {
/// @brief 核对同一个60Hz步的102/118与末尾环境请求；源夹具不读取生产配方。
inline void CompareEffects(const behavior::Actor& actor,const FlandreReference& source) {
    const auto expectedCount=source.effects.size()+source.ambientEffects.size();
    if(actor.effectEvents.size()!=expectedCount) {
        std::cerr<<"External effect at phase "<<actor.phaseFrames<<" count "<<actor.effectEvents.size()<<" vs "<<expectedCount<<'\n';
        std::abort();
    }
    for(std::size_t i=0;i<source.effects.size();++i) {
        const auto& a=actor.effectEvents[i];const auto& e=source.effects[i];
        const auto kind=e.type==3?effects::Kind::Burst:e.type==12?effects::Kind::Flash:e.type==13?effects::Kind::Attached:e.type==17?effects::Kind::Charge:effects::Kind::SlowCharge;
        assert(e.type==3||e.type==12||e.type==13||e.type==17||e.type==18);
        assert(a.kind==kind&&a.count==e.count);
        assert(a.color.r==((e.color>>16)&255)&&a.color.g==((e.color>>8)&255)&&a.color.b==(e.color&255)&&a.color.a==(e.color>>24));
        constexpr float scale=20.0F/448;
        assert(std::abs(a.position.x-(e.position.x-192)*scale)<=5e-4F);
        assert(std::abs(a.position.y+e.position.z*scale)<=5e-4F);
        assert(std::abs(a.position.z-(10-e.position.y*scale))<=5e-4F);
        if(e.type==13) {
            assert(a.axis.x==e.axis.x&&a.axis.y==-e.axis.z&&a.axis.z==-e.axis.y);
            const auto anchor=a.attachment.lock();assert(anchor);
            assert(std::abs(anchor->Radius()-e.radius*scale)<=1e-5F);
        }
    }
    constexpr float scale=20.0F/448;
    for(std::size_t i=0;i<source.ambientEffects.size();++i) {
        const auto& a=actor.effectEvents[source.effects.size()+i];const auto& e=source.ambientEffects[i];
        assert(a.kind==effects::Kind::Ambient&&a.count==1&&a.color.r==48&&a.color.g==48&&a.color.b==255&&a.color.a==255);
        assert(std::abs(a.position.x-(e.position.x-192)*scale)<=5e-4F);
        assert(std::abs(a.position.y+e.position.z*scale)<=5e-4F);
        assert(std::abs(a.position.z-(10-e.position.y*scale))<=5e-4F);
        assert(std::abs(a.velocity.x-e.velocity.x*60*scale)<=1e-4F&&a.velocity.y==0&&std::abs(a.velocity.z+e.velocity.y*60*scale)<=1e-4F);
        assert(std::abs(a.acceleration.x-e.acceleration.x*3600*scale)<=1e-4F&&a.acceleration.y==0&&std::abs(a.acceleration.z+e.acceleration.y*3600*scale)<=1e-4F);
    }
}
}
