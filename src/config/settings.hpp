// 本文件负责声明可持久化的游戏设置及其读写接口。

#pragma once

#include "config/key_bindings.hpp"
#include "app/spell_practice.hpp"

#include <array>
#include <filesystem>

namespace koumakyou {

/// @brief 运行目录中的本地配置文件名。
inline constexpr char kSettingsFilename[] = "koumakyou_settings.cfg";
/// @brief 可选帧率上限；0 表示不限制。
inline constexpr std::array<int, 6> kSupportedFpsLimits{
    30, 60, 120, 144, 240, 0,
};

/// @brief 音乐音量百分比及鼠标灵敏度百分比的取值边界；100% 灵敏度沿用原速度。
inline constexpr int kDefaultMusicVolume = 50;
inline constexpr int kMinMouseSensitivity = 10;
inline constexpr int kMaxMouseSensitivity = 300;
inline constexpr float kBaseMouseSensitivity = 0.0024F;
/// @brief 原典普通关卡的记录槽数；槽号为 1～6，与 StageId 的内部枚举值无关。
inline constexpr int kStageScoreCount = 6;
/// @brief 按标准/挑战模式分别保存整关最高分；御札共享记录，符卡练习和预览不写入。
/// scores 的第一维为 0（标准）/1（挑战），第二维为关卡号减一；分数为非负 64 位整数。
struct StageHighScores {
    std::array<std::array<long long,kStageScoreCount>,2> scores{};
    /// @brief 读取指定模式、1～6 关的最高分；无效关卡返回 0。
    [[nodiscard]] long long Best(bool challenge,int stageNumber) const {
        return stageNumber>=1 && stageNumber<=kStageScoreCount ? scores[challenge][stageNumber-1] : 0;
    }
    /// @brief 尝试提升指定关卡的记录；非法关卡、负分及未超过旧记录时返回 false，不修改数据。
    bool Record(bool challenge,int stageNumber,long long score) {
        if(stageNumber<1 || stageNumber>kStageScoreCount || score<0)return false;
        auto& best=scores[challenge][stageNumber-1];
        if(score<=best)return false;
        best=score;
        return true;
    }
};
/// @brief 普通开局与整关练习的选择；character=0 灵梦/1 魔理沙，shot=0/1 为该角色的两种御札。
/// practicePhase 仅允许 2/4，用于迁移旧符卡练习记录；新菜单独立保存 spellSelection。
/// practiceStage 为 1～6 的关卡号，进入选择页时还须过滤未开放关卡；目前难度仅开放 Normal。
/// 与本局运行状态独立，预览、重开和练习初始化不能覆盖此记录。
struct StartSelection {
    int shot = 0;
    bool challenge = false;
    int practicePhase = 2;
    int practiceStage = 1;
    int character = 0;
};

/// @brief 需要持久化的战斗键位、显示、音量、鼠标、开局资源和选择记录。
struct GameSettings {
    KeyBindings bindings{};
    int fpsLimit = 60;
    int musicVolume = kDefaultMusicVolume; // 0～100，0 静音，只影响 BGM。
    int mouseSensitivity = 100; // 10～300，相对于原始弧度/像素灵敏度的百分比。
    int soundVolume = 60; // 0～100，独立于 BGM。
    int initialLives = 2; // 额外残机 1～5，不含当前正在使用的一条命。
    int initialBombs = 3; // 开局 Bomb 0～3；复活补给保持玩家规则。
    int bestScore = 0; // 本机一面最高分，0～20 亿；练习和挑战不写入。
    StartSelection lastSelection{};
    StageHighScores stageHighScores{};
    spell::Selection spellSelection{}; // 独立保存符卡练习的角色、关卡、符卡组和难度；进入时重置为关卡页。
    spell::Records spellRecords{};
    bool aimAssistEnabled = true; // 开启最多六度的弹道辅助，不修改镜头。
};

/// @brief 读取本地配置；缺失或无效的配置会使用默认值。
/// @param path 配置文件路径。
/// @return 可用于本局游戏的设置。
[[nodiscard]] GameSettings LoadOrCreateSettings(
    const std::filesystem::path& path);
/// @brief 将全部设置保存为 version=9 文本配置；旧版本缺失角色字段时使用灵梦。
/// @param path 配置文件路径。
/// @param settings 待保存的设置。
/// @return 成功写入返回 true，否则返回 false。
[[nodiscard]] bool SaveSettingsFile(const std::filesystem::path& path,
                                    const GameSettings& settings);

}  // namespace koumakyou
