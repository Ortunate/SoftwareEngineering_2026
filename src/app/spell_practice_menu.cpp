// 本文件处理新典式符卡练习浏览、启动门禁、返回选卡与本机取得记录的生命周期。
#include "app/game.hpp"
#include "ui/components.hpp"
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
    if(ui::ArrowButton{layout.Arrow(-1),-1}.Update(input).activated)horizontal=-1;
    if(ui::ArrowButton{layout.Arrow(1),1}.Update(input).activated)horizontal=1;
    if(horizontal)choice.character=(choice.character+spell::kCharacterCount+horizontal)%spell::kCharacterCount;
    bool confirm=input.confirm;
    int vertical=input.vertical;
    const float wheel=GetMouseWheelMove();
    if(wheel!=0) {
        if(choice.page==spell::Page::Stage) {
            for(int index=0;index<spell::kStageCount;++index)
                if(CheckCollisionPointRec(input.mouse,layout.StageRow(index)))vertical=wheel>0?-1:1;
        } else {
            for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups;++index)
                if(CheckCollisionPointRec(input.mouse,layout.GroupRow(index))) {
                    choice.page=spell::Page::Group;vertical=wheel>0?-1:1;
                }
            for(int index=0;index<(choice.stage==6?1:4);++index)
                if(CheckCollisionPointRec(input.mouse,layout.Difficulty(index,choice.stage==6))) {
                    choice.page=spell::Page::Difficulty;vertical=wheel>0?-1:1;
                }
        }
    }
    // Group/Difficulty 仅表示同一页的操作焦点；任何符卡变化都先校正难度，再处理右栏命中。
    // 不能保留新符卡不存在的空难度，否则键盘确认和鼠标直接跨栏会指向不同的符卡。
    const auto normalizeDifficulty=[&] {
        if(spell::Find(choice.stage,choice.group,choice.difficulty))return;
        for(int difficulty=0;difficulty<5;++difficulty)
            if(spell::Find(choice.stage,choice.group,difficulty)) {choice.difficulty=difficulty;return;}
    };
    if(choice.page!=spell::Page::Stage)normalizeDifficulty();
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
            confirm=ui::SelectableRow{layout.StageRow(index),index}.Select(input,choice.stage)||confirm;
        if(choice.stage!=before.stage) {choice.group=0;choice.difficulty=choice.stage==6?4:1;}
    } else {
        const int groups=spell::Stages()[choice.stage].groups;
        const int offset=choice.group/menu::SpellPracticeLayout::kVisibleGroups*menu::SpellPracticeLayout::kVisibleGroups;
        // 鼠标可直接进入任一栏目；只有实际移动或点击才接管焦点，静止指针不覆盖键盘的递进/返回。
        for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups && offset+index<groups;++index) {
            const auto interaction=ui::SelectableRow{layout.GroupRow(index),offset+index}.Update(input,choice.group);
            if(interaction.focused) {
                choice.page=spell::Page::Group;
                confirm=interaction.activated || input.confirm;
            }
        }
        normalizeDifficulty();
        for(int index=0;index<(choice.stage==6?1:4);++index) {
            const int difficulty=choice.stage==6?4:index;
            const auto interaction=ui::SelectableRow{layout.Difficulty(index,choice.stage==6),difficulty,
                spell::Find(choice.stage,choice.group,difficulty)!=nullptr}.Update(input,choice.difficulty);
            if(interaction.focused) {
                choice.page=spell::Page::Difficulty;
                confirm=interaction.activated || input.confirm;
            }
        }
    }
    if(choice.page!=spell::Page::Stage && spell::Stages()[choice.stage].groups>menu::SpellPracticeLayout::kVisibleGroups) {
        for(const int direction:{-1,1})if(input.Clicked(layout.GroupPage(direction))) {
            choice.group=std::clamp(choice.group+direction*menu::SpellPracticeLayout::kVisibleGroups,0,spell::Stages()[choice.stage].groups-1);
            choice.page=spell::Page::Group;normalizeDifficulty();confirm=false;
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
        normalizeDifficulty();
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
