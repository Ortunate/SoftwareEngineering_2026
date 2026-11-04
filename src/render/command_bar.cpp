// 本文件将页面状态和实际绑定转换为底部按键提示；横条的几何与绘制由独立 UI 模块负责。
#include "app/game.hpp"
#include "ui/command_bar.hpp"
#include <array>
#include <string>
#include <utility>

namespace koumakyou {
void Game::DrawCommandBar() const {
    std::array<std::string,ui::CommandBar::kMaxHints> keys;
    std::array<ui::KeyHint,ui::CommandBar::kMaxHints> hints{};
    int count=0;
    // 字符串在本次 Draw 内由固定数组持有，后续填充不会使已借给 KeyHint 的 c_str 失效。
    const auto add=[&](std::string key,const char* action,bool enabled=true) {
        if(count>=ui::CommandBar::kMaxHints)return;
        keys[count]=std::move(key);hints[count]={keys[count].c_str(),action,enabled};++count;
    };
    const auto menu=[&](const char* confirm="确认",const char* back="返回",bool enabled=true) {
        add("WASD / ↑↓←→","选择",enabled);add("鼠标移动","选择",enabled);
        add("Z / Space / 左键",confirm,enabled);add("X / Esc / 右键",back,enabled);
    };
    if(replayMode_==replay::Mode::Playback) {
        add(KeyLabel(bindings_.bomb),replayPaused_?"继续回放":"暂停回放");add("Esc","返回录像列表");
    } else switch(state_) {
        case RunState::ReplayList:
            add("W / S / ↑↓","选择槽位");add("A / D / ←→","翻页");add("Z / Space / 左键","播放");add("X / Esc / 右键","返回");break;
        case RunState::MainMenu:menu("确认","选择退出",titleEntrance_.Finished());break;
        case RunState::Options:
            if(waitingForBinding_) {add("键盘 / 鼠标","修改");add("X / Esc / 右键","取消");}
            else if(optionsPage_==OptionsPage::KeyBindings)menu("修改");
            else {
                add("W / S / ↑↓","选择");add("A / D / ←→","调整");add("鼠标移动","选择");
                add("Z / Space / 左键","确认");add("X / Esc / 右键","返回");
            }
            break;
        case RunState::StartSetup:
            if(spellPractice_) {
                add("W / S / ↑↓",spellSelection_.page==spell::Page::Boss?"选择 Boss":
                    spellSelection_.page==spell::Page::Group?"选择符卡":"选择难度");
                add("A / D / ←→","切换角色");add("鼠标移动","选择");
                add("Z / Space / 左键","确认");add("X / Esc / 右键","返回");
            } else if(setupStep_==StartSetupStep::Ready) {
                add("Z / Space / 左键","开始游戏");add("X / Esc / 右键","返回");
            } else menu();
            break;
        case RunState::MusicRoom:
            add("W / S / ↑↓","选择曲目");add("A / D / ←→","翻页");add("鼠标 / 滚轮","选择");
            add("Z / Space / 左键","播放");add("X / Esc / 右键","返回");break;
        case RunState::Score:
            if(scoreViewStep_==ScoreViewStep::Difficulty)menu("查看成绩","返回");
            else {
                add("A / D / ←→","翻页");add("左右箭头 / 左键","翻页");
                if(scorePageFromRun_)add("Z / Enter / Space","继续");
                add("X / Esc / 右键",scorePageFromRun_?"继续":"选择难度");
            }
            break;
        case RunState::ScoreSave:menu("确认","选择放弃");break;
        case RunState::ScoreName:
            ui::NameEntry({GetScreenWidth(),GetScreenHeight()}).DrawHints({assets_.Common().uiFont});return;
        case RunState::Ending:
            add("Z / Space / 左键","继续");add("Ctrl","快进",endingPlayer_.SeenBefore());
            add("X / Esc / 右键","跳过",endingPlayer_.SeenBefore());break;
        case RunState::Credit:add("Z / Space","返回");add("X / Esc / 右键","返回");break;
        case RunState::Loading:
            if(loadingFailed_) {add("Z / Space / 左键","返回主菜单");add("X / Esc / 右键","返回主菜单");}
            else add("","加载中",false);
            break;
        case RunState::Paused:menu("确认","继续");add("M","返回主菜单");break;
        case RunState::Won: case RunState::GameOver:
            menu("确认",stageId_==StageId::Extra&&!previewMode_?"成绩选存":"返回");
            add("R","重试");add("M",stageId_==StageId::Extra&&!previewMode_?"成绩选存":"返回主菜单");break;
        case RunState::StageClear:
            add("Z / Space / 左键","继续");add("X / Esc / 右键","暂停");add("10s","自动继续");break;
        case RunState::Finishing:add("","等待",false);add("Esc / "+KeyLabel(bindings_.pause),"暂停");break;
        case RunState::Playing:
            if(dialogueActive_) {
                std::string pauseKeys="X / Esc";
                if(bindings_.pause!=KEY_X&&bindings_.pause!=MouseBinding(MOUSE_BUTTON_RIGHT))
                    pauseKeys+=" / "+KeyLabel(bindings_.pause);
                add("Z / Space / 左键","继续");add("Ctrl","跳过");add(std::move(pauseKeys),"暂停");
            } else {
                add(KeyLabel(bindings_.forward)+" / "+KeyLabel(bindings_.backward)+" / "+KeyLabel(bindings_.moveLeft)+" / "+KeyLabel(bindings_.moveRight),"移动");
                add(KeyLabel(bindings_.ascend)+" / "+KeyLabel(bindings_.descend),"升降");
                add(KeyLabel(bindings_.fire)+" / "+KeyLabel(bindings_.focus),"射击 / 低速");
                add(KeyLabel(bindings_.bomb),"符卡");add("鼠标","视角");add("Esc / "+KeyLabel(bindings_.pause),"暂停");
            }
            break;
    }
    const ui::CommandBar bar(GetScreenWidth(),GetScreenHeight());
    bar.Draw({assets_.Common().uiFont,bar.Scale()},std::span(hints).first(count));
}
} // namespace koumakyou
