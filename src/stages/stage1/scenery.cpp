// 本文件绘制一面低多边形森林和湖面；所有装饰位于可玩盒体之外。
#include "stages/stage1/scenery.hpp"
#include "raylib.h"
#include <cmath>
#include <initializer_list>

namespace koumakyou::stage1 {
void DrawScenery(float seconds) {
    // 湖面在下边界外，树干位于左右边界外；不能遮住或占用玩家可移动的体积。
    DrawPlane({0, -10.7F, 20}, {96, 140}, Color{13, 24, 35, 255});
    for (int index = 0; index < 18; ++index) {
        const float z = std::fmod(index * 5.3F - seconds * 0.65F + 960, 96) - 24;
        for (float side : {-1.0F, 1.0F}) {
            const float x = side * (13.4F + (index % 4) * 1.7F);
            const float height = 7.5F + (index % 5) * 0.7F;
            const Color foliage{static_cast<unsigned char>(20 + index % 3 * 3),
                                static_cast<unsigned char>(28 + index % 4 * 3),
                                static_cast<unsigned char>(33 + index % 3 * 4), 255};
            DrawCylinderEx({x, -10.7F, z}, {x, -10.7F + height, z}, 0.22F, 0.16F, 6,
                           Color{32, 24, 32, 255});
            DrawCylinderEx({x, -8.2F, z}, {x, height - 8.2F, z}, 2.6F, 0, 8, foliage);
            DrawCylinderEx({x, -5.8F, z}, {x, height - 6.0F, z}, 1.8F, 0, 8, foliage);
        }
    }
    // 稀疏横向倒影只留在湖面，提供深度线索，不在视线中心铺满导航网格。
    for (int index = 0; index < 24; ++index) {
        const float z = index * 3.0F - 18;
        const float width = 1.2F + std::sin(index * 2.4F + seconds * 0.4F) * 0.35F;
        DrawLine3D({-width, -10.65F, z}, {width, -10.65F, z}, Color{80, 39, 56, 120});
    }
}
}
