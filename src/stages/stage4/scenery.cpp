// 本文件以盒体外的书架、书脊和地板表现四面图书馆，不遮挡场内弹幕。
#include "stages/stage4/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <array>
#include <initializer_list>
namespace koumakyou::stage4 {
void DrawScenery(float seconds) {
    // 室内围护位于可玩盒体之外，遮住公共天空盒；升降和回望仍看到真实屋顶/墙面。
    DrawCube({0,0,95},90,26,1,{35,25,41,255});
    DrawCube({0,12.5F,20},90,1,180,{42,29,45,255});
    DrawCube({0,0,-65},90,26,1,{35,25,41,255});
    for(float side:{-1.0F,1.0F})DrawCube({side*20,0,20},1,26,180,{42,29,45,255});
    DrawPlane({0,-10.7F,25},{100,160},{46,31,43,255});
    constexpr std::array<Color,5> colors{{{82,42,64,255},{48,61,70,255},{93,75,48,255},{47,71,59,255},{75,48,85,255}}};
    for(int row=0;row<12;++row) {
        const float z=std::fmod(row*10.0F-seconds*0.5F+1200,120)-25;
        for(float side:{-1.0F,1.0F}) {
            DrawCube({side*15,-0.5F,z},3,20,8,{48,29,35,255});
            for(int shelf=0;shelf<6;++shelf) {
                const float y=-9.3F+shelf*3.2F;
                DrawCube({side*13.4F,y-0.3F,z},0.3F,0.25F,8,{114,79,62,255});
                for(int book=0;book<8;++book)DrawCube({side*13.2F,y+0.6F,z-3.5F+book},0.2F,1.7F+(book%3)*0.3F,0.7F,colors[(row+shelf+book)%5]);
            }
        }
    }
}
}
