// 本文件绘制 Bomb 背景压暗、角色宣告与名称；世界光球、符阵、星群及光束由统一透明批次绘制。
#include "app/game.hpp"
#include "render/gameplay_layout.hpp"
#include "ui/components.hpp"
#include "ui/typography.hpp"
#include "rlgl.h"
#include <algorithm>

namespace koumakyou {
void Game::DrawPlayerBombBackdrop(float width,float height) const {
    const auto alpha=playerBomb_.BackdropAlpha();
    if(!alpha)return;
    // 临时用正交矩阵覆盖已画好的场景，再恢复 3D；不改变 FBO、视口、相机或后续深度排序。
    // 压暗先于透明特效，否则加色光球和 Master Spark 会连同背景一起被压黑。
    rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
    const auto previousView=rlGetMatrixModelview();
    rlMatrixMode(RL_PROJECTION);rlPushMatrix();rlLoadIdentity();rlOrtho(0,width,height,0,-1,1);
    // rlPushMatrix 在 MODELVIEW 下只切到局部 transform，不能清掉相机 view；须显式替换并恢复。
    Matrix identity{};identity.m0=identity.m5=identity.m10=identity.m15=1;
    rlSetMatrixModelview(identity);
    DrawRectangleRec({0,0,width,height},{0,0,0,alpha});rlDrawRenderBatchActive();
    rlSetMatrixModelview(previousView);rlMatrixMode(RL_PROJECTION);rlPopMatrix();rlMatrixMode(RL_MODELVIEW);
    rlEnableDepthMask();rlEnableDepthTest();
}
void Game::DrawPlayerBombCutIn() const {
    if(!playerBomb_.Active() || !stageContent_ || dialogueActive_)return;
    const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& definition=BombDefinition(playerBomb_.Shot());
    const float elapsed=playerBomb_.Frame()/60.0F;
    const float remaining=(definition.durationFrames-playerBomb_.Frame())/60.0F;
    const float alpha=std::clamp(std::min(elapsed/0.15F,remaining/0.3F),0.0F,1.0F);
    // 继续使用现有重绘自机图；短暂宣告后退下，避免整场 Bomb 遮挡三维目标。
    if(elapsed<1.3F) {
        const float portraitAlpha=alpha*std::clamp((1.3F-elapsed)/0.3F,0.0F,1.0F)*0.75F;
        const auto& visual=assets_.GetVisual(stageContent_->HeroPortraitVisual(0,playerBomb_.Shot()));
        const auto texture=visual.texture;
        if(IsTextureValid(texture)) {
            const auto crop=visual.dialogueCrop;
            const Rectangle source{crop.x*texture.width,crop.y*texture.height,crop.width*texture.width,crop.height*texture.height};
            const auto destination=layout.SpellPortrait(source.width/source.height,1-std::clamp(elapsed/0.3F,0.0F,1.0F));
            DrawTexturePro(texture,source,destination,{},0,Fade(WHITE,portraitAlpha));
        }
    }
    const auto banner=layout.SpellBanner();
    DrawRectangleRounded(banner,0.15F,8,Fade(Color{18,9,29,220},alpha));
    const auto font=assets_.Common().hudFont;
    const float size=ui::FittedSize(font,definition.name.data(),29*layout.scale,banner.width-32*layout.scale);
    ui::DrawCentered(font,definition.name.data(),banner.x+banner.width/2,banner.y+(banner.height-size)/2,size,Fade(Color{255,230,188,255},alpha));
}
} // namespace koumakyou
