// 本文件将六面道中退场后的十秒空档表现为屋顶下沉、红月逼近并归位 +Z 的夜空演出。
#include "stages/stage6/scenery.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>
namespace koumakyou::stage6 {
namespace {
// 两端速度归零，让提前击破和超时退场都能从同一静止背景平滑开始。
float Ease(float value) {
    const float t=std::clamp(value,0.0F,1.0F);
    return t*t*(3-2*t);
}
}
SceneryPose SampleScenery(float transitionSeconds) {
    const float approach=Ease(transitionSeconds/10);
    const float alignment=Ease((transitionSeconds-4)/6);
    return {-90*Ease(transitionSeconds/8),
        {-20*(1-alignment),25-15*alignment,105-53*approach},
        1-Ease((transitionSeconds-2)/6)};
}
void DrawScenery(float seconds,float transitionSeconds) {
    const auto pose=SampleScenery(transitionSeconds);
    const auto moon=pose.moonPosition;
    DrawSphere(moon,16,{180,40,64,255});
    DrawSphere({moon.x-2,moon.y+2,moon.z-14},2.5F,{124,30,50,255});
    DrawSphere({moon.x+7,moon.y-4,moon.z-13},3.4F,{139,33,51,255});
    // 整体变换须覆盖平台、塔身、塔尖和窗户，防止下沉后残留悬空部件。
    const float y=pose.buildingOffsetY;
    DrawPlane({0,-12+y,35},{80,160},{47,32,49,255});
    for(float side:{-1.0F,1.0F})for(int tower=0;tower<7;++tower) {
        const float z=tower*22.0F-15;
        DrawCube({side*22,-9+y,z},10,18,10,{47,29,45,255});
        DrawCylinder({side*22,y,z},0,8,12,4,{75,30,49,255});
        for(int window=0;window<3;++window)DrawCube({side*16.9F,-12+y+window*4.0F,z},0.2F,2.1F,1.1F,{157,68,85,255});
    }
    for(int cloud=0;cloud<9;++cloud) {
        const float x=std::fmod(cloud*19.0F+seconds*0.7F+1800,180)-90;
        DrawCube({x,17+cloud%3*3.0F,45+cloud*7.0F},23,0.15F,7,
            {57,42,61,static_cast<unsigned char>(255*pose.cloudOpacity)});
    }
}
}
