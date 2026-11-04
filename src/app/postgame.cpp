// 本文件连接主线结局、Extra独立出口与成绩选存命名；战斗快照固定后不再推进关卡或修改分数。
#include "app/game.hpp"
#include "render/postgame_layout.hpp"
#include "render/score_layout.hpp"
#include "ui/components.hpp"
#include <algorithm>
#include <array>
#include <chrono>

namespace koumakyou {
void Game::BeginEnding() {
    if(!pendingEnding_)return;
    stage_=StageDirector{};stageContent_.reset();replayStatus_.clear();
    assets_.SetMusicGain(1);
    loadingStarted_=loadingFailed_=false;loadingSeconds_=0;
    state_=RunState::Loading;
    if(IsCursorHidden())ShowCursor();
}
void Game::UpdateEnding(float deltaTime) {
    const auto input=ui::MenuInput::Read();
    if(input.back && endingPlayer_.SeenBefore())endingPlayer_.Skip();
    endingPlayer_.Update(std::max(0.0F,deltaTime),{input.confirm||input.click,
        IsKeyDown(KEY_Z)||IsKeyDown(KEY_SPACE)||IsKeyDown(KEY_ENTER)||IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)});
    assets_.SetMusicGain(endingPlayer_.MusicOpacity());
    if(endingPlayer_.Finished())FinishEnding();
}
void Game::FinishEnding() {
    const auto id=static_cast<std::size_t>(endingPlayer_.Variant());
    scoreProfile_.endingsSeen[id]=true;
    // 观看完成不等于保存本轮分数；放弃命名也应记住该结局已看过，避免下次无法快进。
    scoreSaveError_=score::Save(score::kFilename,scoreProfile_)?"":"观看记录保存失败，请在命名后重试保存";
    BeginScoreSave();
}
void Game::BeginScoreSave() {
    stage_=StageDirector{};stageContent_.reset();replayStatus_.clear();
    assets_.UnloadStage();assets_.SetMusicGain(1);assets_.PlayMenuMusic();pendingEnding_.reset();
    pendingScore_={score_,scoreProfile_.lastName,missCount_,bombCount_,graze_,
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
        StageNumber(stageId_),finishResult_==RunState::Won};
    scoreMode_=challengeMode_?1:0;scoreShot_=static_cast<int>(player_.Shot());scoreDifficulty_=ScoreDifficultyIndex();
    // 分数与Demo共用最近成功保存的名称；首次为空，失败或取消不覆盖已记住的值。
    scoreName_=scoreProfile_.lastName;scoreNameEditor_={.replaceOnType=!scoreName_.empty()};
    selectedScoreSave_=scoreProfile_.Position(scoreMode_!=0,scoreShot_,scoreDifficulty_,pendingScore_.value)?0:1;
    scoreHighlight_=-1;scorePageFromRun_=false;
    state_=RunState::ScoreSave;
    if(IsCursorHidden())ShowCursor();
}
void Game::UpdateScoreSave() {
    const auto input=ui::MenuInput::Read();
    if(input.back) {selectedScoreSave_=1;return;}
    const menu::PostgameLayout layout(GetScreenWidth(),GetScreenHeight());
    const bool qualified=scoreProfile_.Position(scoreMode_!=0,scoreShot_,scoreDifficulty_,pendingScore_.value).has_value();
    const std::array<ui::Item,2> items{{{0,layout.Choice(0),qualified},{1,layout.Choice(1)}}};
    const auto result=ui::UpdateSelection(items,input,selectedScoreSave_);
    if(result.changed)assets_.PlaySoundEffect("menu_move");
    if(!result.confirmed)return;
    assets_.PlaySoundEffect("menu_confirm");
    if(selectedScoreSave_==1){RequestReplayAction(ReplayAction::Menu);return;}
    // 这次确认只打开命名页，不能把同一按键队列写入名字或触发保存。
    while(GetCharPressed()!=0){}
    scoreNameEditor_.selected=ui::NameEntryState::kSave;
    state_=RunState::ScoreName;
}
void Game::SaveNamedScore() {
    if(!score::ValidName(scoreName_)){scoreSaveError_="请输入 1～8 个字符，名字不能全为空格";return;}
    auto candidate=scoreProfile_;
    pendingScore_.name=scoreName_;
    const auto rank=candidate.Position(scoreMode_!=0,scoreShot_,scoreDifficulty_,pendingScore_.value);
    if(!rank || !candidate.Insert(scoreMode_!=0,scoreShot_,scoreDifficulty_,pendingScore_)) {
        scoreSaveError_="本次分数未进入前十";return;
    }
    if(!score::Save(score::kFilename,candidate)) {
        scoreSaveError_="保存失败，请检查目录权限后重试；名字已保留";return;
    }
    scoreProfile_=std::move(candidate);scoreHighlight_=*rank;scorePageFromRun_=true;
    replayNamePrepared_=false; // 成绩确认后的Demo初值必须重新取本次名字，而非先前打开过的录像草稿。
    scoreViewStep_=ScoreViewStep::Records;state_=RunState::Score;selectedMenuItem_=5;assets_.PlaySoundEffect("menu_confirm");
}
bool Game::RememberName(const std::string& name) {
    scoreProfile_.lastName=name;
    return score::Save(score::kFilename,scoreProfile_);
}
void Game::UpdateScoreName() {
    const ui::NameEntry editor({GetScreenWidth(),GetScreenHeight()});
    const auto result=editor.Update(scoreName_,scoreNameEditor_,{score::kNameLength},ui::NameEntryInput::Read());
    if(result.edited)scoreSaveError_.clear();
    if(result.selectionChanged)assets_.PlaySoundEffect("menu_move");
    if(result.action==ui::NameEntryAction::Cancel){state_=RunState::ScoreSave;return;}
    if(result.action==ui::NameEntryAction::Submit)SaveNamedScore();
}
void Game::UpdateScorePage() {
    const auto input=ui::MenuInput::Read();
    if(input.back || (scorePageFromRun_ && input.confirm)) {
        if(scorePageFromRun_)RequestReplayAction(ReplayAction::Menu);
        else if(scoreViewStep_==ScoreViewStep::Records)scoreViewStep_=ScoreViewStep::Difficulty;
        else EnterMainMenu();
        assets_.PlaySoundEffect("menu_cancel");return;
    }
    const menu::ScoreLayout layout(GetScreenWidth(),GetScreenHeight());
    if(scoreViewStep_==ScoreViewStep::Difficulty) {
        std::array<ui::Item,score::kDifficulties> items{};
        for(int i=0;i<score::kDifficulties;++i)items[i]={i,layout.Difficulty(i)};
        const auto result=ui::UpdateSelection(items,input,scoreDifficulty_);
        if(result.changed)assets_.PlaySoundEffect("menu_move");
        if(result.confirmed) {
            scoreViewStep_=ScoreViewStep::Records;
            assets_.PlaySoundEffect("menu_confirm");
        }
        return;
    }
    // 每页同时展示两种模式，只按左右翻机体页；同帧键盘和箭头点击合并为一次翻页。
    int direction=input.horizontal;
    for(int arrow:{-1,1})if(ui::ArrowButton{layout.Arrow(arrow),arrow}.Update(input).activated)direction=arrow;
    if(direction) {
        scoreShot_=(scoreShot_+direction+score::kShots)%score::kShots;
        scoreHighlight_=-1;assets_.PlaySoundEffect("menu_move");
    }
}
} // namespace koumakyou
