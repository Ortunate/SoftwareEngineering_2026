// 本文件处理新典式符卡练习浏览、启动门禁、返回选卡与本机取得记录的生命周期。
#include "app/game.hpp"
#include "app/ui_input.hpp"
#include "render/spell_practice_layout.hpp"
namespace koumakyou {
void Game::UpdateSpellPractice() {
    const auto input=ui::MenuInput::Read();
    const menu::SpellPracticeLayout layout(GetScreenWidth(),GetScreenHeight());
    auto& choice=spellSelection_;
    if(input.back) {
        assets_.PlaySoundEffect("menu_cancel");
        if(choice.page==spell::Page::Stage)EnterMainMenu();
        else choice.page=choice.page==spell::Page::Group?spell::Page::Stage:spell::Page::Group;
        return;
    }
    const auto before=choice;
    int horizontal=input.horizontal;
    // 箭头只切换组合，绝不能将同一鼠标点击继续当作列表确认。
    if(input.Clicked(layout.Arrow(-1)))horizontal=-1;
    if(input.Clicked(layout.Arrow(1)))horizontal=1;
    if(horizontal)choice.character=(choice.character+spell::kCharacterCount+horizontal)%spell::kCharacterCount;
    bool confirm=input.confirm;
    int vertical=input.vertical;
    const float wheel=GetMouseWheelMove();
    if(wheel!=0 && CheckCollisionPointRec(input.mouse,layout.canvas.Rect(32,layout.compact?590:148,layout.compact?696:860,layout.compact?650:490)))
        vertical=wheel>0?-1:1;
    const int count=choice.page==spell::Page::Stage?spell::kStageCount:
        choice.page==spell::Page::Group?spell::Stages()[choice.stage].groups:choice.stage==6?1:4;
    int& selected=choice.page==spell::Page::Stage?choice.stage:choice.page==spell::Page::Group?choice.group:choice.difficulty;
    if(vertical) {
        if(choice.page==spell::Page::Difficulty) {
            // 原典不存在的难度是空槽；灰色但存在的变体仍可浏览完整名称与记录。
            int candidate=choice.stage==6?0:choice.difficulty;
            do { candidate=(candidate+count+vertical)%count; }
            while(!spell::Find(choice.stage,choice.group,choice.stage==6?4:candidate));
            choice.difficulty=choice.stage==6?4:candidate;
        } else selected=(selected+count+vertical)%count;
    }
    if(choice.page==spell::Page::Stage) {
        for(int index=0;index<spell::kStageCount;++index)
            confirm=input.Select(layout.StageRow(index),index,choice.stage)||confirm;
        if(choice.stage!=before.stage) {choice.group=0;choice.difficulty=choice.stage==6?4:1;}
    } else if(choice.page==spell::Page::Group) {
        const int offset=choice.group/menu::SpellPracticeLayout::kVisibleGroups*menu::SpellPracticeLayout::kVisibleGroups;
        for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups && offset+index<count;++index)
            confirm=input.Select(layout.GroupRow(index),offset+index,choice.group)||confirm;
    } else {
        for(int index=0;index<count;++index) {
            const int difficulty=choice.stage==6?4:index;
            if(spell::Find(choice.stage,choice.group,difficulty))
                confirm=input.Select(layout.Difficulty(index,choice.stage==6),difficulty,choice.difficulty)||confirm;
        }
        // 选择难度后依然能点击左列直接返回对应符卡组，无须多按一次取消。
        const int offset=choice.group/menu::SpellPracticeLayout::kVisibleGroups*menu::SpellPracticeLayout::kVisibleGroups;
        for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups && offset+index<spell::Stages()[choice.stage].groups;++index)
            if(input.Clicked(layout.GroupRow(index))) {choice.group=offset+index;choice.page=spell::Page::Group;confirm=false;}
    }
    if(choice.page!=spell::Page::Stage && spell::Stages()[choice.stage].groups>menu::SpellPracticeLayout::kVisibleGroups) {
        for(const int direction:{-1,1})if(input.Clicked(layout.GroupPage(direction))) {
            choice.group=std::clamp(choice.group+direction*menu::SpellPracticeLayout::kVisibleGroups,0,spell::Stages()[choice.stage].groups-1);
            choice.page=spell::Page::Group;confirm=false;
        }
    }
    if(before.stage!=choice.stage || before.group!=choice.group || before.character!=choice.character || before.difficulty!=choice.difficulty) {
        assets_.PlaySoundEffect("menu_move");SaveSettings();
    }
    if(horizontal)return;
    if(!confirm)return;
    if(choice.page==spell::Page::Stage)choice.page=spell::Page::Group;
    else if(choice.page==spell::Page::Group) {
        choice.page=spell::Page::Difficulty;
        if(!spell::Find(choice.stage,choice.group,choice.difficulty))
            for(int difficulty=0;difficulty<5;++difficulty)if(spell::Find(choice.stage,choice.group,difficulty)) {choice.difficulty=difficulty;break;}
    } else {
        const auto* card=spell::Find(choice.stage,choice.group,choice.difficulty);
        if(!spell::Available(card,choice.character,spellRecords_)) {assets_.PlaySoundEffect("menu_cancel");return;}
        selectedShot_=choice.character;
        practicePhase_=card->phase;
        stageId_=StageId::Stage1;
        challengeMode_=false;
        SaveSettings();
        StartRun();
    }
    if(state_==RunState::StartSetup)SaveSettings();
    menuMotion_.Confirm();assets_.PlaySoundEffect("menu_confirm");
}

void Game::ReturnToSpellPractice() {
    EnterMainMenu();
    practiceMode_=spellPractice_=true;challengeMode_=false;
    selectedMenuItem_=3;
    setupStep_=StartSetupStep::Spell;
    spellSelection_.page=spell::Page::Group;
    state_=RunState::StartSetup;
    startSetupMotion_.Advance(static_cast<int>(setupStep_),1);
}

void Game::BeginSpellRecord() {
    if(spellAttemptOpen_ || previewMode_ || dialogueActive_ || !boss_ || !boss_->active || !boss_->SpellName())return;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if(shotTestActive_)return;
#endif
    const int number=boss_->phase==2?3:boss_->phase==4?7:0;
    if(StageNumber(stageId_)!=1 || number==0)return;
    spellAttemptNumber_=number;spellAttemptCharacter_=selectedShot_;
    spellAttemptOpen_=true;spellAttemptPractice_=spellPractice_;
    spellRecords_.Begin(spellAttemptCharacter_,number,spellPractice_);
    SaveSettings();
}

void Game::FinishSpellRecord(bool capture) {
    if(!spellAttemptOpen_)return;
    spellRecords_.Finish(spellAttemptCharacter_,spellAttemptNumber_,spellAttemptPractice_,capture,score_);
    spellAttemptOpen_=false;
    if(spellAttemptPractice_)spellCaptured_=capture;
    SaveSettings();
}
} // namespace koumakyou
