// 本文件验证设置持久化、版本迁移、键鼠编码与冲突交换，不需要打开窗口。
#include "config/settings.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <limits>

/// @brief 验证默认值、持久化边界、旧配置和键鼠冲突；失败时断言终止。
void RunSettingsTests() {
    using namespace koumakyou;
    const auto path = std::filesystem::temp_directory_path() /
        ("koumakyou_settings_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".cfg");
    const auto defaults = LoadOrCreateSettings(path);
    assert(std::filesystem::exists(path));
    assert(defaults.fpsLimit == 60 && defaults.musicVolume == 50);
    assert(defaults.mouseSensitivity == 100);
    assert(defaults.soundVolume == 60 && defaults.initialLives == 2 && defaults.initialBombs == 3);
    assert(defaults.lastSelection.shot == 0 && !defaults.lastSelection.challenge && defaults.lastSelection.practicePhase == 2);
    assert(defaults.bindings.focus == MouseBinding(MOUSE_BUTTON_RIGHT));
    assert(defaults.bindings.descend == KEY_LEFT_SHIFT);
    assert(defaults.bindings.fire == MouseBinding(MOUSE_BUTTON_LEFT));
    assert(defaults.bindings.bomb == KEY_F && defaults.bindings.pause == KEY_P);
    static_assert(kBindingMembers.size() == 10);
    // 往返覆盖静音/最大音量及灵敏度两端，避免边界在重启后回退。
    for (const int volume : {0, 35, 100}) {
        for (const int sensitivity : {10, 170, 300}) {
            auto settings = defaults;
            settings.fpsLimit = 144;
            settings.musicVolume = volume;
            settings.mouseSensitivity = sensitivity;
            settings.soundVolume = volume;
            settings.initialLives = 5;
            settings.initialBombs = 0;
            settings.bestScore = 1234567;
            settings.bindings.fire = KEY_F1;
            settings.bindings.pause = MouseBinding(MOUSE_BUTTON_EXTRA);
            assert(SaveSettingsFile(path, settings));
            const auto loaded = LoadOrCreateSettings(path);
            assert(loaded.fpsLimit == 144 && loaded.musicVolume == volume);
            assert(loaded.mouseSensitivity == sensitivity);
            assert(loaded.soundVolume == volume && loaded.initialLives == 5 && loaded.initialBombs == 0);
            assert(loaded.bestScore == 1234567);
            assert(loaded.bindings.fire == KEY_F1);
            assert(loaded.bindings.pause == MouseBinding(MOUSE_BUTTON_EXTRA));
        }
    }
    auto bindings = defaults.bindings;
    assert(Rebind(bindings, 6, MouseBinding(MOUSE_BUTTON_LEFT)));
    assert(bindings.focus == MouseBinding(MOUSE_BUTTON_LEFT));
    assert(bindings.fire == MouseBinding(MOUSE_BUTTON_RIGHT));
    assert(Rebind(bindings, 7, KEY_W));
    assert(bindings.fire == KEY_W && bindings.forward == MouseBinding(MOUSE_BUTTON_RIGHT));
    assert(!Rebind(bindings, -1, KEY_A));
    assert(!Rebind(bindings, 10, KEY_A));
    for (int invalid : std::array<int, 10>{KEY_NULL, KEY_ESCAPE, KEY_F3, KEY_F4, KEY_F5, KEY_F6, -1, 999, 1007, 99999})
        assert(!Rebind(bindings, 0, invalid));
    for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_BACK; ++button)
        assert(IsValidBinding(MouseBinding(button)));
    // 旧默认 C 升级为右键；旧自定义 Focus 和 FPS 保留。
    for (int oldFocus : {KEY_C, KEY_V}) {
        { std::ofstream out(path); out << "version=1\nfps_limit=240\nfocus=" << oldFocus << '\n'; }
        const auto migrated = LoadOrCreateSettings(path);
        assert(migrated.fpsLimit == 240);
        assert(migrated.bindings.focus == (oldFocus == KEY_C ? MouseBinding(MOUSE_BUTTON_RIGHT) : KEY_V));
        assert(migrated.musicVolume == 50 && migrated.mouseSensitivity == 100);
        assert(LoadOrCreateSettings(path).bindings.focus == migrated.bindings.focus);
    }
    { std::ofstream out(path); out << "version=2\nfocus=67\n"; }
    assert(LoadOrCreateSettings(path).bindings.focus == KEY_C);
    { std::ofstream out(path); out << "version=2\nmusic_volume=101\nmouse_sensitivity=0\nfire=1007\nfps_limit=13\n"; }
    const auto invalid = LoadOrCreateSettings(path);
    assert(invalid.musicVolume == 50 && invalid.mouseSensitivity == 100 && invalid.fpsLimit == 60);
    assert(invalid.bindings.fire == MouseBinding(MOUSE_BUTTON_LEFT));
    { std::ofstream out(path); out << "version=2\nmusic_volume=-1\nmouse_sensitivity=301\n"; }
    assert(LoadOrCreateSettings(path).mouseSensitivity == 100);
    { std::ofstream out(path); out << "version=2\nmusic_volume=nan\nmouse_sensitivity=100junk\nforward=1000\n"; }
    const auto repaired = LoadOrCreateSettings(path);
    assert(repaired.musicVolume == 50 && repaired.mouseSensitivity == 100);
    assert(repaired.bindings.forward == KEY_W); // 与射击主键重复，回退键位组。
    { std::ofstream out(path); out << "version=3\nsound_volume=101\ninitial_lives=0\ninitial_bombs=4\nbest_score=-1\n"; }
    const auto stock = LoadOrCreateSettings(path);
    assert(stock.soundVolume == 60 && stock.initialLives == 2 && stock.initialBombs == 3 && stock.bestScore == 0);
    // 完整旧默认配置升级，同时清除备用字段；新版自定义旧默认键码必须能往返保存。
    { std::ofstream out(path); out << "version=3\nfocus=1003\nfire=90\nbomb=88\npause=80\nfire_alt=1000\nbomb_alt=1001\n"; }
    const auto migrated = LoadOrCreateSettings(path);
    assert(migrated.bindings.focus == MouseBinding(MOUSE_BUTTON_RIGHT));
    assert(migrated.bindings.fire == MouseBinding(MOUSE_BUTTON_LEFT));
    assert(migrated.bindings.bomb == KEY_F && migrated.bindings.pause == KEY_P);
    { std::ifstream in(path); const std::string content((std::istreambuf_iterator<char>(in)), {});
      assert(content.find("version=6") != std::string::npos);
      assert(content.find("fire_alt") == std::string::npos && content.find("bomb_alt") == std::string::npos); }
    { std::ofstream out(path); out << "version=3\nfocus=86\nfire=81\nbomb=69\npause=79\n"; }
    const auto custom = LoadOrCreateSettings(path);
    assert(custom.bindings.focus == KEY_V && custom.bindings.fire == KEY_Q);
    assert(custom.bindings.bomb == KEY_E && custom.bindings.pause == KEY_O);
    // 新默认已被自定义动作占用时，不让迁移重置整组玩家键位。
    { std::ofstream out(path); out << "version=3\nfocus=1003\nfire=1001\nbomb=88\n"; }
    const auto occupied = LoadOrCreateSettings(path);
    assert(occupied.bindings.focus == MouseBinding(MOUSE_BUTTON_SIDE));
    assert(occupied.bindings.fire == MouseBinding(MOUSE_BUTTON_RIGHT) && occupied.bindings.bomb == KEY_F);
    { std::ofstream out(path); out << "version=4\nfocus=1003\nfire=90\nbomb=88\n"; }
    const auto current = LoadOrCreateSettings(path);
    assert(current.bindings.focus == MouseBinding(MOUSE_BUTTON_SIDE));
    assert(current.bindings.fire == KEY_Z && current.bindings.bomb == KEY_X);
    // 全部开局选项组合均需跨文件往返；更新配置不能再次迁移 version=4 的自定义键位。
    for (int shot : {0,1}) for (bool challenge : {false,true}) for (int phase : {2,4}) {
        auto settings = current;
        settings.lastSelection = {shot,challenge,phase};
        assert(SaveSettingsFile(path,settings));
        const auto loaded = LoadOrCreateSettings(path);
        assert(loaded.lastSelection.shot == shot && loaded.lastSelection.challenge == challenge);
        assert(loaded.lastSelection.practicePhase == phase && loaded.bindings.fire == KEY_Z);
    }
    for (const char* fields : {"last_shot=-1\nlast_challenge=2\nlast_practice_phase=3\n",
                              "last_shot=2\nlast_challenge=-1\nlast_practice_phase=0\n",
                              "last_shot=nan\nlast_challenge=true\nlast_practice_phase=4junk\n"}) {
        { std::ofstream out(path); out << "version=5\n" << fields; }
        const auto loaded = LoadOrCreateSettings(path);
        assert(loaded.lastSelection.shot == 0 && !loaded.lastSelection.challenge && loaded.lastSelection.practicePhase == 2);
    }
    { std::ofstream out(path); out << "version=4\nfocus=1003\nfire=90\nbomb=88\nbest_score=1234\n"; }
    const auto upgraded = LoadOrCreateSettings(path);
    assert(upgraded.lastSelection.shot == 0 && !upgraded.lastSelection.challenge && upgraded.lastSelection.practicePhase == 2);
    assert(upgraded.bestScore == 1234 && upgraded.bindings.fire == KEY_Z && upgraded.bindings.bomb == KEY_X);
    assert(upgraded.stageHighScores.Best(false,1)==1234 && upgraded.stageHighScores.Best(true,1)==0);
    // 分模式、分关卡记录必须独立往返，且不得被较低分、非法槽或新配置误迁移覆盖。
    auto records=defaults;
    for(int mode=0;mode<2;++mode)for(int stage=1;stage<=kStageScoreCount;++stage) {
        const long long score=5000000000LL+mode*1000000+stage*123;
        assert(records.stageHighScores.Record(mode!=0,stage,score));
        assert(!records.stageHighScores.Record(mode!=0,stage,score-1));
    }
    assert(!records.stageHighScores.Record(false,0,999) && !records.stageHighScores.Record(true,7,999));
    assert(!records.stageHighScores.Record(false,1,-1));
    assert(records.stageHighScores.Best(false,0)==0 && records.stageHighScores.Best(true,7)==0);
    records.bestScore=2000000000;
    records.stageHighScores.scores[1][5]=std::numeric_limits<long long>::max();
    for(int stage=1;stage<=kStageScoreCount;++stage) {
        records.lastSelection.practiceStage=stage;
        assert(SaveSettingsFile(path,records));
        const auto loaded=LoadOrCreateSettings(path);
        assert(loaded.stageHighScores.scores==records.stageHighScores.scores);
        assert(loaded.lastSelection.practiceStage==stage);
    }
    for(const char* invalid : {"-1","9223372036854775808","120junk","nan"}) {
        { std::ofstream out(path); out << "version=6\nlast_practice_stage=7\nstage_best_standard_1=" << invalid
              << "\nstage_best_challenge_2=8765432109\nstage_best_standard_0=123\nstage_best_standard_7=456\n"; }
        const auto loaded=LoadOrCreateSettings(path);
        assert(loaded.stageHighScores.Best(false,1)==0 && loaded.stageHighScores.Best(true,2)==8765432109LL);
        assert(loaded.lastSelection.practiceStage==1);
    }
    { std::ofstream out(path); out << "version=5\nbest_score=5176180\nlast_shot=1\nlast_challenge=1\n"; }
    const auto previous=LoadOrCreateSettings(path);
    assert(previous.stageHighScores.Best(false,1)==5176180 && previous.stageHighScores.Best(true,1)==0);
    assert(previous.lastSelection.shot==1 && previous.lastSelection.challenge && previous.lastSelection.practiceStage==1);
    assert(LoadOrCreateSettings(path).stageHighScores.scores==previous.stageHighScores.scores);
    std::filesystem::remove(path);
}
