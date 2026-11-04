// 本文件处理按 Boss 归属的符卡练习浏览、启动门禁、返回选卡与本机取得记录的生命周期。
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
        if(choice.page==spell::Page::Boss)EnterMainMenu();
        else choice.page=choice.page==spell::Page::Group?spell::Page::Boss:spell::Page::Group;
        return;
    }
    const auto before=choice;
    int boss=spell::BossIndex(choice.stage,choice.group);
    int group=spell::BossGroupIndex(choice.stage,choice.group);
    // 浏览行号仅在 UI 内使用；所有退出路径仍保存原关卡/组身份，不能写进战斗或录像。
    const auto selectGroup=[&](int index) {
        const auto entry=spell::Bosses()[boss].groups[index];
        choice.stage=entry.stage;choice.group=entry.group;group=index;
        if(spell::Find(choice.stage,choice.group,choice.difficulty))return;
        for(int difficulty=0;difficulty<5;++difficulty)
            if(spell::Find(choice.stage,choice.group,difficulty)) {choice.difficulty=difficulty;return;}
    };
    if(boss<0) {boss=0;selectGroup(0);}
    int horizontal=input.horizontal;
    // 箭头只切换组合，绝不能将同一鼠标点击继续当作列表确认。
    if(ui::ArrowButton{layout.Arrow(-1),-1}.Update(input).activated)horizontal=-1;
    if(ui::ArrowButton{layout.Arrow(1),1}.Update(input).activated)horizontal=1;
    if(horizontal)choice.character=(choice.character+spell::kCharacterCount+horizontal)%spell::kCharacterCount;
    bool confirm=input.confirm;
    int vertical=input.vertical;
    const float wheel=GetMouseWheelMove();
    if(wheel!=0) {
        if(choice.page==spell::Page::Boss) {
            for(int index=0;index<spell::kBossCount;++index)
                if(CheckCollisionPointRec(input.mouse,layout.BossRow(index)))vertical=wheel>0?-1:1;
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
    if(choice.page==spell::Page::Boss) {
        const int previous=boss;
        if(vertical)boss=(boss+spell::kBossCount+vertical)%spell::kBossCount;
        for(int index=0;index<spell::kBossCount;++index)
            confirm=ui::SelectableRow{layout.BossRow(index),index}.Select(input,boss)||confirm;
        if(boss!=previous) {
            choice.difficulty=1;selectGroup(0);
        }
    } else {
        selectGroup(group);
        const int groups=static_cast<int>(spell::Bosses()[boss].groups.size());
        if(vertical) {
            if(choice.page==spell::Page::Group)selectGroup((group+groups+vertical)%groups);
            else {
                // 同一 Boss 可含主线与 Extra；难度槽取决于符卡实际关卡，不能按 Boss 行号推断。
                const int count=choice.stage==6?1:4;
                int candidate=choice.stage==6?0:choice.difficulty;
                do {candidate=(candidate+count+vertical)%count;}
                while(!spell::Find(choice.stage,choice.group,choice.stage==6?4:candidate));
                choice.difficulty=choice.stage==6?4:candidate;
            }
        }
        const int offset=group/menu::SpellPracticeLayout::kVisibleGroups*menu::SpellPracticeLayout::kVisibleGroups;
        // 静止指针不覆盖键盘焦点；实际移动或点击才允许直接跨栏。
        for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups && offset+index<groups;++index) {
            const auto interaction=ui::SelectableRow{layout.GroupRow(index),offset+index}.Update(input,group);
            if(interaction.focused) {
                selectGroup(group);choice.page=spell::Page::Group;
                confirm=interaction.activated || input.confirm;
            }
        }
        for(int index=0;index<(choice.stage==6?1:4);++index) {
            const int difficulty=choice.stage==6?4:index;
            const auto interaction=ui::SelectableRow{layout.Difficulty(index,choice.stage==6),difficulty,
                spell::Find(choice.stage,choice.group,difficulty)!=nullptr}.Update(input,choice.difficulty);
            if(interaction.focused) {
                choice.page=spell::Page::Difficulty;
                confirm=interaction.activated || input.confirm;
            }
        }
        if(groups>menu::SpellPracticeLayout::kVisibleGroups) {
            for(const int direction:{-1,1})if(input.Clicked(layout.GroupPage(direction))) {
                selectGroup(std::clamp(group+direction*menu::SpellPracticeLayout::kVisibleGroups,0,groups-1));
                choice.page=spell::Page::Group;confirm=false;
            }
        }
    }
    if(before.stage!=choice.stage || before.group!=choice.group || before.character!=choice.character || before.difficulty!=choice.difficulty) {
        assets_.PlaySoundEffect("menu_move");SaveSettings();
    }
    if(horizontal || !confirm)return;
    if(choice.page==spell::Page::Boss) {selectGroup(group);choice.page=spell::Page::Group;}
    else if(choice.page==spell::Page::Group)choice.page=spell::Page::Difficulty;
    else {
        const auto* card=spell::Find(choice.stage,choice.group,choice.difficulty);
        if(!spell::Available(card,choice.character,spellRecords_)) {assets_.PlaySoundEffect("menu_cancel");return;}
        selectedCharacter_=choice.character/2;selectedShot_=choice.character%2;
        practiceAttack_=card->phase;
        stageId_=*SpellPracticeStage(card->stage);
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
    if(replayMode_==replay::Mode::Playback)return;
    if(spellAttemptOpen_ || previewMode_ || dialogueActive_ || !boss_ || !boss_->active || !boss_->SpellName())return;
#ifdef KOUMAKYOU_DEBUG_FEATURES
    if(shotTestActive_)return;
#endif
    const int number=boss_->SpellCardNumber();
    if(number<1||number>spell::kRecordCount||(spellPractice_&&number>spell::kSpellCount))return;
    spellAttemptNumber_=number;spellAttemptCharacter_=static_cast<int>(player_.Shot());
    spellAttemptOpen_=true;spellAttemptPractice_=spellPractice_;
    spellRecords_.Begin(spellAttemptCharacter_,number,spellPractice_);
    SaveSettings();
}

void Game::FinishSpellRecord(bool capture) {
    if(replayMode_==replay::Mode::Playback)return;
    if(!spellAttemptOpen_)return;
    spellRecords_.Finish(spellAttemptCharacter_,spellAttemptNumber_,spellAttemptPractice_,capture,score_);
    spellAttemptOpen_=false;
    if(spellAttemptPractice_)spellCaptured_=capture;
    SaveSettings();
}
} // namespace koumakyou
