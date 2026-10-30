// 本文件将Extra源图集的青绿地砖、白雾和双侧卷纹适配为盒体外场景，不复刻源相机投影。
#include "stages/stage_extra/scenery.hpp"
#include "rlgl.h"
#include <array>
#include <cmath>
#include <initializer_list>

namespace koumakyou::stage_extra {
namespace {
// UV来自256×256逻辑ANM图集，高清DDS只提高采样密度，不能改变裁图范围。
constexpr Rectangle kTile{17.0F/256,17.0F/256,32.0F/256,32.0F/256};
constexpr Rectangle kFog{96.0F/256,1.0F/256,64.0F/256,62.0F/256};
constexpr Rectangle kScroll{168.0F/256,1.0F/256,64.0F/256,47.0F/256};

// 顶点依次对应UV左上、左下、右下、右上；调用方已关闭背面剔除，允许从盒体内看两侧装饰。
void Quad(Texture2D atlas,Rectangle uv,const std::array<Vector3,4>& points,Color tint,bool mirror=false) {
    const float left=uv.x+(mirror?uv.width:0),right=uv.x+(mirror?0:uv.width);
    rlSetTexture(atlas.id);rlBegin(RL_QUADS);rlColor4ub(tint.r,tint.g,tint.b,tint.a);
    rlTexCoord2f(left,uv.y);rlVertex3f(points[0].x,points[0].y,points[0].z);
    rlTexCoord2f(left,uv.y+uv.height);rlVertex3f(points[1].x,points[1].y,points[1].z);
    rlTexCoord2f(right,uv.y+uv.height);rlVertex3f(points[2].x,points[2].y,points[2].z);
    rlTexCoord2f(right,uv.y);rlVertex3f(points[3].x,points[3].y,points[3].z);
    rlEnd();
}
}
void DrawScenery(float seconds,Texture2D atlas) {
    if(atlas.id==0||!std::isfinite(seconds)||seconds<0)return;
    // 世界范围X/Y为±10、Z为±11.5；地面低于-10、侧卷纹在±16，雾层在±24。
    // 这是保留源图案和纵深层次的三维适配，不把源像素与相机轨迹带入运行时坐标。
    DrawCube({0,0,110},160,90,1,{8,24,30,255});
    DrawCube({0,0,-50},160,90,1,{8,24,30,255});
    DrawCube({0,28,30},160,1,160,{8,24,30,255});
    DrawCube({-48,0,30},1,90,160,{8,24,30,255});
    DrawCube({48,0,30},1,90,160,{8,24,30,255});
    DrawPlane({0,-10.85F,30},{96,160},{9,37,44,255});
    const float offset=static_cast<float>(std::fmod(static_cast<double>(seconds)*0.7,8.0));
    rlDisableBackfaceCulling();
    for(int row=0;row<20;++row) {
        const float z=row*8.0F-offset-40;
        for(int column=0;column<12;++column) {
            const float x=column*8.0F-48;
            Quad(atlas,kTile,{{{x,-10.8F,z+8},{x,-10.8F,z},{x+8,-10.8F,z},{x+8,-10.8F,z+8}}},{67,99,106,255});
        }
    }
    // 半透明图案从远到近提交，关闭深度写入以免图集透明边缘挡住随后绘制的演员/弹体。
    rlSetTexture(0);rlDrawRenderBatchActive();rlDisableDepthMask();
    for(int row=19;row>=0;--row) {
        const float z=row*8.0F-offset-40;
        for(float side:{-1.0F,1.0F}) {
            const float x=side*16;
            Quad(atlas,kScroll,{{{x,-3.8F,z+8},{x,-10.7F,z+8},{x,-10.7F,z},{x,-3.8F,z}}},{75,105,114,210},side<0);
            if(row%3==0) {
                const float fogX=side*24;
                Quad(atlas,kFog,{{{fogX,16,z+12},{fogX,-10.6F,z+12},{fogX,-10.6F,z-12},{fogX,16,z-12}}},{79,119,128,24},side<0);
            }
        }
    }
    rlSetTexture(0);rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();
}
}
