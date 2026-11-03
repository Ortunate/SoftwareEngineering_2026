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
    assert(defaults.mouseSensitivity == 100 && defaults.aimAssistEnabled);
    for(bool enabled:{false,true}) {
        auto settings=defaults;settings.aimAssistEnabled=enabled;
        assert(SaveSettingsFile(path,settings));
        assert(LoadOrCreateSettings(path).aimAssistEnabled==enabled);
    }
    for(const char* config:{"version=8\nbest_score=42\n", "version=9\naim_assist=5\n"}) {
        { std::ofstream out(path);out<<config; }
        assert(LoadOrCreateSettings(path).aimAssistEnabled);
    }
    assert(defaults.soundVolume == 60 && defaults.initialLives == 3 && defaults.initialBombs == 3);
    assert(defaults.lastSelection.character == 0 && defaults.lastSelection.shot == 0 && !defaults.lastSelection.challenge && defaults.lastSelection.practicePhase == 2);
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
    assert(stock.soundVolume == 60 && stock.initialLives == 3 && stock.initialBombs == 3 && stock.bestScore == 0);
    // 完整旧默认配置升级，同时清除备用字段；新版自定义旧默认键码必须能往返保存。
    { std::ofstream out(path); out << "version=3\nfocus=1003\nfire=90\nbomb=88\npause=80\nfire_alt=1000\nbomb_alt=1001\n"; }
    const auto migrated = LoadOrCreateSettings(path);
    assert(migrated.bindings.focus == MouseBinding(MOUSE_BUTTON_RIGHT));
    assert(migrated.bindings.fire == MouseBinding(MOUSE_BUTTON_LEFT));
    assert(migrated.bindings.bomb == KEY_F && migrated.bindings.pause == KEY_P);
    { std::ifstream in(path); const std::string content((std::istreambuf_iterator<char>(in)), {});
      assert(content.find("version=9") != std::string::npos);
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
    for (int character : {0,1}) for (int shot : {0,1}) for (bool challenge : {false,true}) for (int phase : {2,4}) {
        auto settings = current;
        settings.lastSelection = {shot,challenge,phase,1,character,3};
        assert(SaveSettingsFile(path,settings));
        const auto loaded = LoadOrCreateSettings(path);
        assert(loaded.lastSelection.shot == shot && loaded.lastSelection.challenge == challenge);
        assert(loaded.lastSelection.character==character);
        assert(loaded.lastSelection.difficulty==3);
        assert(loaded.lastSelection.practicePhase == phase && loaded.bindings.fire == KEY_Z);
    }
    for (const char* fields : {"last_character=-1\nlast_shot=-1\nlast_challenge=2\nlast_practice_phase=3\nlast_difficulty=-1\n",
                              "last_character=2\nlast_shot=2\nlast_challenge=-1\nlast_practice_phase=0\nlast_difficulty=4\n",
                              "last_character=nan\nlast_shot=nan\nlast_challenge=true\nlast_practice_phase=4junk\n"}) {
        { std::ofstream out(path); out << "version=5\n" << fields; }
        const auto loaded = LoadOrCreateSettings(path);
        assert(loaded.lastSelection.character==0);
        assert(loaded.lastSelection.difficulty==1);
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
    assert(records.stageHighScores.Record(false,1,456,0));
    assert(records.stageHighScores.Best(false,1,0)==456 && records.stageHighScores.Best(false,1,1)!=456);
    records.stageHighScores.scores[1][5]=std::numeric_limits<long long>::max();
    for(int stage=1;stage<=kStageScoreCount;++stage) {
        records.lastSelection.practiceStage=stage;
        assert(SaveSettingsFile(path,records));
        const auto loaded=LoadOrCreateSettings(path);
        assert(loaded.stageHighScores.scores==records.stageHighScores.scores);
        assert(loaded.stageHighScores.difficultyScores==records.stageHighScores.difficultyScores);
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
    // 跨关卡归属只影响浏览，旧配置的原始身份仍能恢复到正确 Boss 与局部行。
    for(const auto identity:{spell::Group{5,0},spell::Group{6,0},spell::Group{6,12}}) {
        auto remembered=previous;
        remembered.spellSelection={spell::Page::Boss,identity.stage,identity.group,identity.stage==6?4:1,2};
        assert(SaveSettingsFile(path,remembered));
        const auto loaded=LoadOrCreateSettings(path).spellSelection;
        assert(loaded.stage==identity.stage&&loaded.group==identity.group&&loaded.character==2);
        assert(spell::BossIndex(loaded.stage,loaded.group)==(identity.stage==5?4:identity.group==0?3:6));
    }
    // 符卡记录按原典编号和四种角色组合独立保存，旧整关记录不能被迁移为符卡取得。
    auto spells=previous;
    spells.spellSelection={spell::Page::Boss,3,15,3,3};
    spells.spellRecords.Begin(0,3,true);spells.spellRecords.Finish(0,3,true,true,8765432109LL);
    spells.spellRecords.Begin(1,7,false);spells.spellRecords.Finish(1,7,false,true,9999);
    spells.spellRecords.Begin(2,4,false);spells.spellRecords.Begin(3,131,false);
    for(int character=0;character<4;++character)for(int number=132;number<=134;++number) {
        spells.spellRecords.Begin(character,number,false);
        spells.spellRecords.Finish(character,number,false,number!=133,9999);
    }
    assert(SaveSettingsFile(path,spells));
    const auto restored=LoadOrCreateSettings(path);
    assert(restored.spellSelection.stage==3 && restored.spellSelection.group==15 && restored.spellSelection.character==3);
    assert(restored.spellRecords.Get(0,3).bestScore==8765432109LL && restored.spellRecords.Get(0,3).captured==1);
    assert(restored.spellRecords.Get(1,7).mainCaptured==1 && restored.spellRecords.Get(1,7).bestScore==0);
    assert(restored.spellRecords.Seen(1,7) && !restored.spellRecords.Seen(0,7) && !restored.spellRecords.Seen(0,3));
    assert(spell::Available(spell::Find(0,2,1),1,restored.spellRecords));
    assert(!spell::Available(spell::Find(0,2,1),0,restored.spellRecords));
    assert(!spell::Available(spell::Find(0,1,1),0,restored.spellRecords));
    for(int character=0;character<spell::kCharacterCount;++character) {
        assert(restored.spellRecords.Seen(character,4)==(character==2));
        assert(restored.spellRecords.Seen(character,131)==(character==3));
    }
    assert(restored.spellRecords.Get(1,3).attempts==0 && restored.stageHighScores.scores==previous.stageHighScores.scores);
    for(int character=0;character<4;++character)for(int number=132;number<=134;++number) {
        const auto& record=restored.spellRecords.Get(character,number);
        assert(record.mainAttempts==1&&record.mainCaptured==(number!=133));
        assert(record.attempts==0&&record.captured==0&&record.bestScore==0);
    }
    {std::ofstream out(path);out<<"version=7\nlast_spell_stage=3\nlast_spell_group=99\nlast_spell_character=4\n"
        <<"spell_record_0_3=0 3 -1 1 1 0 0\nspell_record_1_7=1 7 100 1 2 0 0\n"
        <<"spell_record_wrong=0 7 100 1 1 0 0\nspell_record_3_131=3 131 9999999999 2 1 0 0\n"
        <<"spell_record_0_132=0 132 5 1 1 1 1\nspell_record_0_135=0 135 0 0 0 1 1\n";}
    const auto invalidSpell=LoadOrCreateSettings(path);
    assert(!invalidSpell.spellRecords.Seen(0,132)&&!invalidSpell.spellRecords.Seen(0,135));
    assert(invalidSpell.spellSelection.group==1 && invalidSpell.spellSelection.character==0);
    assert(invalidSpell.spellRecords.Get(0,3).attempts==0 && invalidSpell.spellRecords.Get(1,7).attempts==0);
    assert(invalidSpell.spellRecords.Get(0,7).attempts==0 && invalidSpell.spellRecords.Get(3,131).bestScore==9999999999LL);
    for(int character=0;character<spell::kCharacterCount;++character)
        assert(!invalidSpell.spellRecords.Seen(character,3) && !invalidSpell.spellRecords.Seen(character,7) && !invalidSpell.spellRecords.Seen(character,131));
    std::filesystem::remove(path);
}
