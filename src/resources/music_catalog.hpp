// 本文件集中定义 Music Room 的展示顺序与运行时音乐路径，不加载关卡或持有音频流。
#pragma once

#include <array>

namespace koumakyou::music {
/// @brief 一首可试听的BGM；字符串借用静态存储，path相对运行时素材根目录，role说明实际使用场景。
struct Track { const char* title; const char* role; const char* path; };
/// @brief 标题、六面各自的道中/Boss、Extra三槽、结局；仅收录现有BGM，不混入音效或重复加载关卡素材。
inline constexpr std::array<Track,17> kTracks{{
    {"比赤色更鲜红的梦","标题","common/比赤色更鲜红的梦.mp3"},
    {"宛如鬼灯的鲜红之魂","Stage 1 · 道中","stages/stage1/宛如鬼灯的鲜红之魂.mp3"},
    {"妖魔夜行","Stage 1 · Boss","stages/stage1/妖魔夜行.mp3"},
    {"Lunate Elf","Stage 2 · 道中","stages/stage2/stage_theme.mp3"},
    {"活泼的纯情小姑娘","Stage 2 · Boss","stages/stage2/boss_theme.mp3"},
    {"上海红茶馆 ～ Chinese Tea","Stage 3 · 道中","stages/stage3/stage_theme.mp3"},
    {"明治十七年的上海爱丽丝","Stage 3 · Boss","stages/stage3/boss_theme.mp3"},
    {"巴瓦鲁魔法图书馆","Stage 4 · 道中","stages/stage4/stage_theme.mp3"},
    {"Locked Girl ～ 少女密室","Stage 4 · Boss","stages/stage4/boss_theme.mp3"},
    {"女仆与血之怀表","Stage 5 · 道中","stages/stage5/stage_theme.mp3"},
    {"月时计 ～ Luna Dial","Stage 5 · Boss","stages/stage5/boss_theme.mp3"},
    {"特佩斯的年幼末裔","Stage 6 · 道中","stages/stage6/stage_theme.mp3"},
    {"献给已逝公主的七重奏","Stage 6 · Boss","stages/stage6/boss_theme.mp3"},
    {"魔法少女们的百年祭","Extra · 道中","stages/stage_extra/stage_theme.mp3"},
    {"U.N.Owen就是她吗？","Extra · Boss","stages/stage_extra/boss_theme.mp3"},
    {"绯红恶魔姗姗而至","Extra · 事件","stages/stage_extra/event_theme.mp3"},
    {"结局主题曲","结局","endings/ending.mp3"},
}};
} // namespace koumakyou::music
