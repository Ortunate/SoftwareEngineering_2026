// 本文件以盒体外湖面、岸边冰晶及低亮度水纹呈现二面场景，保留可玩体积的弹幕可见性。
#include "stages/stage2/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <initializer_list>
namespace koumakyou::stage2 {
void DrawScenery(float seconds) {
    DrawPlane({0,-10.7F,20},{100,150},{17,40,55,255});
    for(int index=0;index<22;++index) {
        const float z=std::fmod(index*4.7F-seconds*0.7F+960,100)-24;
        for(float side:{-1.0F,1.0F}) {
            const float x=side*(13.5F+index%4*1.5F);
            const float height=3.0F+index%5*1.2F;
            DrawCylinderEx({x,-10.7F,z},{x,-10.7F+height,z},1.5F,0,5,{46,84,102,255});
        }
    }
    for(int index=0;index<30;++index) {
        const float z=index*3.0F-24,width=1.8F+std::sin(index*2.1F+seconds*0.3F)*0.5F;
        DrawLine3D({-width,-10.65F,z},{width,-10.65F,z},{66,116,137,110});
    }
}
}
