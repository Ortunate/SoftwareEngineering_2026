// 本文件处理开局的统一键鼠确认、逐级返回和选项切换；选择记忆独立持久化。
#include "app/game.hpp"
#include "app/ui_input.hpp"
#include "render/start_setup_layout.hpp"

namespace koumakyou {
static_assert(kPracticeStages.size()==kStageScoreCount);
void Game::UpdateStartSetup() {
    const menu::StartSetupLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto input=ui::MenuInput::Read();
    if (input.back) {
        assets_.PlaySoundEffect("menu_cancel");
        switch(setupStep_) {
            case StartSetupStep::Difficulty: case StartSetupStep::Spell: EnterMainMenu(); break;
            case StartSetupStep::Character: setupStep_=StartSetupStep::Difficulty; break;
            case StartSetupStep::Shot: setupStep_=StartSetupStep::Character; break;
            case StartSetupStep::Mode: setupStep_=StartSetupStep::Shot; break;
            case StartSetupStep::Stage: setupStep_=StartSetupStep::Mode; break;
            case StartSetupStep::Ready: setupStep_=spellPractice_?StartSetupStep::Spell:
                practiceMode_?StartSetupStep::Stage:StartSetupStep::Mode; break;
            case StartSetupStep::Count: return;
        }
        return;
    }
    bool confirm=input.confirm;
    if(setupStep_==StartSetupStep::Difficulty)confirm=confirm || input.Clicked(layout.Difficulty(1));
    else if(setupStep_==StartSetupStep::Character)
        confirm=confirm || input.Clicked(layout.Portrait()) || input.Clicked(layout.CharacterInfo());
    else if(setupStep_==StartSetupStep::Ready)confirm=confirm || input.Clicked(layout.ReadyButton());
    if(setupStep_==StartSetupStep::Stage) {
        const int previous=selectedPracticeStage_;
        if(input.Direction()!=0) {
            // 跳过未注册的关卡；目录始终包含第一面，避免导航落入不可启动的槽或无限循环。
            do { selectedPracticeStage_=(selectedPracticeStage_+kStageScoreCount+input.Direction())%kStageScoreCount; }
            while(!kPracticeStages[selectedPracticeStage_]);
        }
        for(int index=0;index<kStageScoreCount;++index)
            if(kPracticeStages[index])confirm=input.Select(layout.StageRow(index),index,selectedPracticeStage_) || confirm;
        if(previous!=selectedPracticeStage_) {
            assets_.PlaySoundEffect("menu_move");
            lastSelection_.practiceStage=selectedPracticeStage_+1;
            SaveSettings();
        }
    }
    if(setupStep_==StartSetupStep::Shot || setupStep_==StartSetupStep::Mode || setupStep_==StartSetupStep::Spell) {
        const int previous=setupStep_==StartSetupStep::Shot?selectedShot_:setupStep_==StartSetupStep::Mode?int(challengeMode_):int(practicePhase_==4);
        int selected=input.Direction()!=0?1-previous:previous;
        for(int index=0;index<2;++index)
            confirm=input.Select(layout.Choice(index),index,selected) || confirm;
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
    // 点击先选择再推进当前页；每帧只处理一个步骤，持续按住左键或确认键不会穿过最终确认页。
    switch(setupStep_) {
        case StartSetupStep::Difficulty: setupStep_=StartSetupStep::Character; break;
        case StartSetupStep::Character: setupStep_=StartSetupStep::Shot; break;
        case StartSetupStep::Shot: setupStep_=StartSetupStep::Mode; break;
        case StartSetupStep::Mode: setupStep_=practiceMode_?StartSetupStep::Stage:StartSetupStep::Ready; break;
        case StartSetupStep::Spell: setupStep_=StartSetupStep::Ready; break;
        case StartSetupStep::Stage:
            if(!kPracticeStages[selectedPracticeStage_])return;
            stageId_=*kPracticeStages[selectedPracticeStage_];
            setupStep_=StartSetupStep::Ready;
            break;
        case StartSetupStep::Ready: StartRun(); break;
        case StartSetupStep::Count: return;
    }
}
} // namespace koumakyou
