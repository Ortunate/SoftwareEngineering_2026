// 本文件用盒体外的红毯、窗框、立柱与吊灯表现五面走廊，保留场内三维移动和弹幕可读性。
#include "stages/stage5/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <initializer_list>
namespace koumakyou::stage5 {
void DrawScenery(float seconds) {
    DrawPlane({0,-10.8F,30},{60,180},{56,38,44,255});
    DrawCube({0,-10.7F,30},16,0.1F,180,{100,20,39,255});
    DrawCube({0,13,30},44,1,180,{36,25,38,255});
    DrawCube({0,0,115},44,28,1,{42,25,37,255});
    DrawCube({0,0,-60},44,28,1,{42,25,37,255});
    for(float side:{-1.0F,1.0F}) {
        DrawCube({side*21,0,30},1,28,180,{48,30,46,255});
        for(int row=0;row<14;++row) {
            const float z=std::fmod(row*11.0F-seconds*0.7F+1540,154)-32;
            DrawCube({side*18.8F,0,z},1.6F,25,1.6F,{115,89,97,255});
            DrawCube({side*20.35F,1,z+5.5F},0.25F,12,7,{48,56,88,255});
            DrawCube({side*20.1F,1,z+5.5F},0.4F,0.3F,7,{158,127,130,255});
            DrawCube({side*20.1F,1,z+5.5F},0.4F,12,0.3F,{158,127,130,255});
            DrawCube({side*18.8F,-9.9F,z},2.4F,1.5F,2.4F,{157,121,125,255});
            if(side>0) {
                DrawCylinder({0,9,z},0.12F,0.12F,4,6,{118,100,77,255});
                DrawSphere({0,6.9F,z},0.5F,{246,189,118,255});
            }
        }
    }
}
}
