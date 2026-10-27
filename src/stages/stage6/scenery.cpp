// 本文件以红月、尖顶与薄云表现六面夜空，所有装饰均位于可玩体积之外。
#include "stages/stage6/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <initializer_list>
namespace koumakyou::stage6 {
void DrawScenery(float seconds) {
    DrawSphere({-20,25,105},16,{180,40,64,255});
    DrawSphere({-22,27,91},2.5F,{124,30,50,255});
    DrawSphere({-13,21,92},3.4F,{139,33,51,255});
    DrawPlane({0,-12,35},{80,160},{47,32,49,255});
    for(float side:{-1.0F,1.0F})for(int tower=0;tower<7;++tower) {
        const float z=tower*22.0F-15;
        DrawCube({side*22,-9,z},10,18,10,{47,29,45,255});
        DrawCylinder({side*22,0,z},0,8,12,4,{75,30,49,255});
        for(int window=0;window<3;++window)DrawCube({side*16.9F,-12+window*4.0F,z},0.2F,2.1F,1.1F,{157,68,85,255});
    }
    for(int cloud=0;cloud<9;++cloud) {
        const float x=std::fmod(cloud*19.0F+seconds*0.7F+1800,180)-90;
        DrawCube({x,17+cloud%3*3.0F,45+cloud*7.0F},23,0.15F,7,{57,42,61,255});
    }
}
}
