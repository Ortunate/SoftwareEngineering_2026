// 本文件连接主线结局、Extra独立出口与成绩选存命名；战斗快照固定后不再推进关卡或修改分数。
#include "app/game.hpp"
#include "render/postgame_layout.hpp"
#include "ui/components.hpp"
#include <algorithm>
#include <array>
#include <chrono>

namespace koumakyou {
void Game::BeginEnding() {
    if(!pendingEnding_)return;
    stage_=StageDirector{};stageContent_.reset();
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
    stage_=StageDirector{};stageContent_.reset();
    assets_.UnloadStage();assets_.SetMusicGain(1);assets_.PlayMenuMusic();pendingEnding_.reset();
    pendingScore_={score_,scoreProfile_.lastName,missCount_,bombCount_,graze_,
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()};
    scoreMode_=challengeMode_?1:0;scoreShot_=static_cast<int>(player_.Shot());scoreDifficulty_=ScoreDifficultyIndex();
    scoreName_=scoreProfile_.lastName;replaceScoreName_=true;
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
    if(selectedScoreSave_==1){EnterMainMenu();return;}
    // 这次确认只打开命名页，不能把同一按键队列写入名字或触发保存。
    while(GetCharPressed()!=0){}
    state_=RunState::ScoreName;
}
void Game::AppendScoreName(char value) {
    if(value<32 || value>126)return;
    if(replaceScoreName_){scoreName_.clear();replaceScoreName_=false;}
    if(scoreName_.size()<score::kNameLength)scoreName_+=value;
    scoreSaveError_.clear();
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
    state_=RunState::Score;selectedMenuItem_=5;assets_.PlaySoundEffect("menu_confirm");
}
void Game::UpdateScoreName() {
    // 命名只消费字符和编辑键，不走菜单选项导航；WASD、Z、X、空格均可直接写入名字。
    // 取消优先于输入和保存，鼠标左键或方向键不能追加字符、触发保存。
    if(IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){state_=RunState::ScoreSave;return;}
    if((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) {
        replaceScoreName_=true;while(GetCharPressed()!=0){}
    }
    else for(int character=GetCharPressed();character!=0;character=GetCharPressed())
        if(character>=32 && character<=126)AppendScoreName(static_cast<char>(character));
    if(IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE)) {
        replaceScoreName_=false;if(!scoreName_.empty())scoreName_.pop_back();scoreSaveError_.clear();
    }
    if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_ENTER))SaveNamedScore();
}
void Game::UpdateScorePage() {
    const auto input=ui::MenuInput::Read();
    if(input.back || (scorePageFromRun_ && input.confirm)){EnterMainMenu();return;}
    const menu::PostgameLayout layout(GetScreenWidth(),GetScreenHeight());
    int modeChange=input.vertical;
    int shotChange=input.horizontal;
    int difficultyChange=IsKeyPressed(KEY_Q)?-1:IsKeyPressed(KEY_E)?1:0;
    if(input.Clicked(layout.ModeArrow(-1)) || input.Clicked(layout.ModeArrow(1)))modeChange=1;
    if(input.Clicked(layout.ShotArrow(-1)))shotChange=-1;
    if(input.Clicked(layout.ShotArrow(1)))shotChange=1;
    if(input.Clicked(layout.DifficultyArrow(-1)))difficultyChange=-1;
    if(input.Clicked(layout.DifficultyArrow(1)))difficultyChange=1;
    if(modeChange){scoreMode_=1-scoreMode_;scoreHighlight_=-1;}
    if(shotChange){scoreShot_=(scoreShot_+shotChange+4)%4;scoreHighlight_=-1;}
    if(difficultyChange){scoreDifficulty_=(scoreDifficulty_+difficultyChange+5)%5;scoreHighlight_=-1;}
    if(modeChange || shotChange || difficultyChange)assets_.PlaySoundEffect("menu_move");
}
} // namespace koumakyou
