// 本文件处理开局的键盘确认、逐级返回和鼠标选项；最终确认页支持点击开局，选择记忆独立持久化。
#include "app/game.hpp"
#include "render/start_setup_layout.hpp"

namespace koumakyou {
void Game::UpdateStartSetup() {
    const menu::StartSetupLayout layout(GetScreenWidth(),GetScreenHeight());
    const bool pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const auto clicked=[&](Rectangle bounds) { return pressed && CheckCollisionPointRec(GetMousePosition(),bounds); };
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        assets_.PlaySoundEffect("menu_cancel");
        switch(setupStep_) {
            case StartSetupStep::Difficulty: case StartSetupStep::Spell: EnterMainMenu(); break;
            case StartSetupStep::Character: setupStep_=StartSetupStep::Difficulty; break;
            case StartSetupStep::Shot: setupStep_=StartSetupStep::Character; break;
            case StartSetupStep::Mode: setupStep_=StartSetupStep::Shot; break;
            case StartSetupStep::Ready: setupStep_=spellPractice_?StartSetupStep::Spell:StartSetupStep::Mode; break;
        }
        return;
    }
    const bool confirm=IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        (setupStep_==StartSetupStep::Ready && clicked(layout.ReadyButton()));
    if(setupStep_==StartSetupStep::Shot || setupStep_==StartSetupStep::Mode || setupStep_==StartSetupStep::Spell) {
        const int previous=setupStep_==StartSetupStep::Shot?selectedShot_:setupStep_==StartSetupStep::Mode?int(challengeMode_):int(practicePhase_==4);
        int selected=previous;
        const auto delta=GetMouseDelta();
        for(int index=0;index<2;++index)
            if(CheckCollisionPointRec(GetMousePosition(),layout.Choice(index)) && (pressed || delta.x!=0 || delta.y!=0))selected=index;
        if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))selected=1-selected;
        if(selected!=previous) {
            assets_.PlaySoundEffect("menu_move");
            if(setupStep_==StartSetupStep::Shot)lastSelection_.shot=selectedShot_=selected;
            else if(setupStep_==StartSetupStep::Mode)lastSelection_.challenge=challengeMode_=selected==1;
            else lastSelection_.practicePhase=practicePhase_=selected==0?2:4;
            SaveSettings();
        }
    }
    if(!confirm)return;
    menuMotion_.Confirm(); assets_.PlaySoundEffect("menu_confirm");
    // 只沿显式页面推进；鼠标开局仅限最终确认页的 START GAME，按住确认不会穿过两页进入游戏。
    switch(setupStep_) {
        case StartSetupStep::Difficulty: setupStep_=StartSetupStep::Character; break;
        case StartSetupStep::Character: setupStep_=StartSetupStep::Shot; break;
        case StartSetupStep::Shot: setupStep_=StartSetupStep::Mode; break;
        case StartSetupStep::Mode: case StartSetupStep::Spell: setupStep_=StartSetupStep::Ready; break;
        case StartSetupStep::Ready: StartRun(); break;
    }
}
} // namespace koumakyou
