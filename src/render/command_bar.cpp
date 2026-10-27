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
    switch(state_) {
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
                add("W / S / ↑↓",spellSelection_.page==spell::Page::Stage?"选择关卡":
                    spellSelection_.page==spell::Page::Group?"选择符卡":"选择难度");
                add("A / D / ←→","切换角色");add("鼠标移动","选择");
                add("Z / Space / 左键","确认");add("X / Esc / 右键","返回");
            } else if(setupStep_==StartSetupStep::Ready) {
                add("Z / Space / 左键","开始游戏");add("X / Esc / 右键","返回");
            } else menu();
            break;
        case RunState::MusicRoom:menu("播放");break;
        case RunState::Score:
            add("W / S / ↑↓","切换模式");add("A / D / ←→","切换御札");add("X / Esc / 右键","返回");break;
        case RunState::ScoreSave:menu("确认","选择放弃");break;
        case RunState::ScoreName:
            add("键盘字符","输入名字");add("Enter","保存");add("Ctrl+A","重新输入");
            add("Backspace","删除");add("Esc / 右键","返回选存");break;
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
            menu();add("R","重试");add("M","返回主菜单");break;
        case RunState::StageClear:
            add("Z / Space / 左键","继续");add("X / Esc / 右键","暂停");add("10s","自动继续");break;
        case RunState::Finishing:add("","等待",false);add("Esc / "+KeyLabel(bindings_.pause),"暂停");break;
        case RunState::Playing:
            if(dialogueActive_) {
                add("Z / Space / 左键","继续");add("Ctrl","跳过");add("X / Esc / 右键","暂停");
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
