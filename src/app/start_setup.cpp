// 本文件处理开局的统一键鼠确认、逐级返回和选项切换；选择记忆独立持久化。
#include "app/game.hpp"
#include "app/ui_input.hpp"
#include "render/start_setup_layout.hpp"

namespace koumakyou {
static_assert(kPracticeStages.size()==kStageScoreCount);
void Game::UpdateStartSetup() {
    if(spellPractice_) {UpdateSpellPractice();return;}
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
    else if(setupStep_==StartSetupStep::Character) {
        const int previous=selectedCharacter_;
        if(input.Direction()!=0)selectedCharacter_=1-selectedCharacter_;
        bool arrow=false;
        for(int direction:{-1,1})if(input.Clicked(layout.CharacterArrow(direction))) {
            selectedCharacter_=1-previous;arrow=true;
        }
        if(!arrow) {
            confirm=confirm || input.Clicked(layout.Portrait()) || input.Clicked(layout.CharacterInfo());
        }
        if(previous!=selectedCharacter_) {
            lastSelection_.character=selectedCharacter_;SaveSettings();assets_.PlaySoundEffect("menu_move");
        }
        if(arrow)return; // 一次箭头点击不能同时确认角色；命中不随人物淡入淡出改变。
    }
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
    if(setupStep_==StartSetupStep::Shot || setupStep_==StartSetupStep::Mode) {
        const int previous=setupStep_==StartSetupStep::Shot?selectedShot_:int(challengeMode_);
        int selected=input.Direction()!=0?1-previous:previous;
        for(int index=0;index<2;++index)
            confirm=input.Select(layout.Choice(index),index,selected) || confirm;
        if(selected!=previous) {
            assets_.PlaySoundEffect("menu_move");
            if(setupStep_==StartSetupStep::Shot)lastSelection_.shot=selectedShot_=selected;
            else lastSelection_.challenge=challengeMode_=selected==1;
            SaveSettings();
        }
    }
    if(!confirm)return;
    // 魔理沙候选可完整浏览两种御札和后续页面，但实战尚未接通，不能悄悄启动灵梦的机体。
    if(setupStep_==StartSetupStep::Ready && selectedCharacter_!=0) {assets_.PlaySoundEffect("menu_cancel");return;}
    menuMotion_.Confirm(); assets_.PlaySoundEffect("menu_confirm");
    // 点击先选择再推进当前页；每帧只处理一个步骤，持续按住左键或确认键不会穿过最终确认页。
    switch(setupStep_) {
        case StartSetupStep::Difficulty: setupStep_=StartSetupStep::Character; break;
        case StartSetupStep::Character: setupStep_=StartSetupStep::Shot; break;
        case StartSetupStep::Shot: setupStep_=StartSetupStep::Mode; break;
        case StartSetupStep::Mode: setupStep_=practiceMode_?StartSetupStep::Stage:StartSetupStep::Ready; break;
        case StartSetupStep::Spell: return; // 由上方独立入口处理，不能借普通 Ready 绕过符卡门禁。
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
