// 本文件用盒体外的石径、树篱和门柱呈现红魔馆庭院；场内保持无遮挡的弹幕可见性。
#include "stages/stage3/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <initializer_list>
namespace koumakyou::stage3 {
void DrawScenery(float seconds) {
    DrawPlane({0,-10.7F,25},{100,160},{33,43,31,255});
    DrawCube({0,-10.6F,25},12,0.1F,160,{79,64,64,255});
    for(int i=0;i<24;++i) {
        const float z=std::fmod(i*5.0F-seconds+1000,120)-25;
        DrawCubeWires({0,-10.5F,z},12,0.1F,4.5F,{100,82,77,160});
        for(float side:{-1.0F,1.0F}) {
            DrawCube({side*14,-7.5F,z},3,6.4F,3,{54,72,41,255});
            if(i%4==0) {DrawCube({side*11,-5.7F,z},1.4F,10,1.4F,{92,67,66,255});DrawSphere({side*11,-0.5F,z},0.8F,{126,87,78,255});}
        }
    }
}
}
