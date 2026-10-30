// 本文件负责显示、音量、开局资源、分模式关卡成绩、开局选择与统一绑定设置的读取、校验和保存。

#include "config/settings.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace koumakyou {
namespace {

/// @brief 将配置文件中的字段名映射到 KeyBindings 成员。
struct BindingField {
    std::string_view name;
    int KeyBindings::*member;
};

constexpr std::array<BindingField, 10> kBindingFields{{
    {"forward", &KeyBindings::forward},
    {"backward", &KeyBindings::backward},
    {"move_left", &KeyBindings::moveLeft},
    {"move_right", &KeyBindings::moveRight},
    {"ascend", &KeyBindings::ascend},
    {"descend", &KeyBindings::descend},
    {"focus", &KeyBindings::focus},
    {"fire", &KeyBindings::fire},
    {"bomb", &KeyBindings::bomb},
    {"pause", &KeyBindings::pause},
}};

/// @brief 去掉配置行首尾的空白字符。
/// @param text 原始文本视图。
/// @return 指向去空白后内容的视图。
std::string_view Trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// @brief 严格解析十进制整数，不接受剩余非数字字符。
/// @param text 待解析文本。
/// @return 成功时返回整数，失败时返回空值。
template<typename Integer=int>
std::optional<Integer> ParseInteger(std::string_view text) {
    text = Trim(text);
    Integer value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// @brief 检查 FPS 限制是否是界面提供的档位。
/// @param value 候选帧率上限。
/// @return 属于支持列表时返回 true。
bool IsSupportedFpsLimit(int value) {
    return std::find(kSupportedFpsLimits.begin(), kSupportedFpsLimits.end(),
                     value) != kSupportedFpsLimits.end();
}

/// @brief 检查全部战斗按键有效且无重复。
/// @param bindings 待检查的键位映射。
/// @return 每个动作都有唯一有效键码时返回 true。
bool HasUniqueBindings(const KeyBindings& bindings) {
    for (std::size_t left = 0; left < kBindingFields.size(); ++left) {
        const int leftKey = bindings.*(kBindingFields[left].member);
        if (!IsValidBinding(leftKey)) return false;
        for (std::size_t right = left + 1; right < kBindingFields.size(); ++right) {
            if (leftKey == bindings.*(kBindingFields[right].member)) return false;
        }
    }
    return true;
}

}  // namespace

GameSettings LoadOrCreateSettings(const std::filesystem::path& path) {
    GameSettings settings{};
    std::error_code fileError;
    if (!std::filesystem::exists(path, fileError) || fileError) {
        (void)SaveSettingsFile(path, settings);
        return settings;
    }

    std::ifstream input(path);
    if (!input) return settings;

    bool shouldRewrite = false;
    int version = 1;
    std::string line;
    while (std::getline(input, line)) {
        const std::string_view trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string_view::npos) {
            shouldRewrite = true;
            continue;
        }

        const std::string_view name = Trim(trimmed.substr(0, separator));
        if(name.starts_with("spell_record_")) {
            // 稀疏行：角色、原典符卡号、最高分、练习尝试/取得、本篇尝试/取得；整行有效才接收。
            std::istringstream row{std::string(trimmed.substr(separator+1))};
            int character=0,number=0; spell::Record record; std::string trailing;
            if(row>>character>>number>>record.bestScore>>record.attempts>>record.captured>>record.mainAttempts>>record.mainCaptured &&
               !(row>>trailing) && character>=0 && character<spell::kCharacterCount && number>=1 && number<=spell::kRecordCount &&
               name=="spell_record_"+std::to_string(character)+"_"+std::to_string(number) &&
               record.bestScore>=0 && record.attempts>=0 && record.attempts<=2000000000 && record.captured>=0 && record.captured<=record.attempts &&
               (number<=spell::kSpellCount || (record.bestScore==0&&record.attempts==0&&record.captured==0)) &&
               record.mainAttempts>=0 && record.mainAttempts<=2000000000 && record.mainCaptured>=0 && record.mainCaptured<=record.mainAttempts)
                settings.spellRecords.values[character][number-1]=record;
            else shouldRewrite=true;
            continue;
        }
        bool stageField=false;
        for(int mode=0;mode<2;++mode) for(int stage=1;stage<=kStageScoreCount;++stage) {
            const std::string key=std::string(mode?"stage_best_challenge_":"stage_best_standard_")+std::to_string(stage);
            if(name!=key)continue;
            const auto score=ParseInteger<long long>(trimmed.substr(separator+1));
            if(score && *score>=0)settings.stageHighScores.scores[mode][stage-1]=*score;
            else shouldRewrite=true;
            stageField=true;
        }
        for(int difficulty:{0,2,3})for(int mode=0;mode<2;++mode)for(int stage=1;stage<=kStageScoreCount;++stage) {
            const std::string key="stage_best_"+std::to_string(difficulty)+"_"+
                (mode?"challenge_":"standard_")+std::to_string(stage);
            if(name!=key)continue;
            const auto score=ParseInteger<long long>(trimmed.substr(separator+1));
            if(score && *score>=0)settings.stageHighScores.difficultyScores[difficulty][mode][stage-1]=*score;
            else shouldRewrite=true;
            stageField=true;
        }
        if(stageField)continue;
        const std::optional<int> value = ParseInteger(trimmed.substr(separator + 1));
        if (!value) {
            shouldRewrite = true;
            continue;
        }

        if (name == "version") {
            version = *value;
            shouldRewrite = shouldRewrite || version != 8;
        } else if (name == "music_volume") {
            if (*value >= 0 && *value <= 100) settings.musicVolume = *value;
            else shouldRewrite = true;
        } else if (name == "mouse_sensitivity") {
            if (*value >= kMinMouseSensitivity && *value <= kMaxMouseSensitivity)
                settings.mouseSensitivity = *value;
            else shouldRewrite = true;
        } else if (name == "sound_volume") {
            if (*value >= 0 && *value <= 100) settings.soundVolume = *value;
            else shouldRewrite = true;
        } else if (name == "initial_lives") {
            if (*value >= 1 && *value <= 5) settings.initialLives = *value;
            else shouldRewrite = true;
        } else if (name == "initial_bombs") {
            if (*value >= 0 && *value <= 3) settings.initialBombs = *value;
            else shouldRewrite = true;
        } else if (name == "best_score") {
            if (*value >= 0 && *value <= 2000000000) settings.bestScore = *value;
            else shouldRewrite = true;
        } else if (name == "last_shot") {
            if (*value == 0 || *value == 1) settings.lastSelection.shot = *value;
            else shouldRewrite = true;
        } else if (name == "last_character") {
            if (*value == 0 || *value == 1) settings.lastSelection.character = *value;
            else shouldRewrite = true;
        } else if (name == "last_difficulty") {
            if (*value >= 0 && *value < 4) settings.lastSelection.difficulty = *value;
            else shouldRewrite = true;
        } else if (name == "last_challenge") {
            if (*value == 0 || *value == 1) settings.lastSelection.challenge = *value == 1;
            else shouldRewrite = true;
        } else if (name == "last_practice_phase") {
            if (*value == 2 || *value == 4) settings.lastSelection.practicePhase = *value;
            else shouldRewrite = true;
        } else if (name == "last_practice_stage") {
            if (*value >= 1 && *value <= kStageScoreCount) settings.lastSelection.practiceStage = *value;
            else shouldRewrite = true;
        } else if (name == "last_spell_character") {
            if(*value>=0 && *value<spell::kCharacterCount)settings.spellSelection.character=*value;else shouldRewrite=true;
        } else if (name == "last_spell_stage") {
            if(*value>=0 && *value<spell::kStageCount)settings.spellSelection.stage=*value;else shouldRewrite=true;
        } else if (name == "last_spell_group") {
            if(*value>=0 && *value<16)settings.spellSelection.group=*value;else shouldRewrite=true;
        } else if (name == "last_spell_difficulty") {
            if(*value>=0 && *value<5)settings.spellSelection.difficulty=*value;else shouldRewrite=true;
        } else if (name == "fps_limit") {
            if (IsSupportedFpsLimit(*value)) {
                settings.fpsLimit = *value;
            } else {
                shouldRewrite = true;
            }
        } else {
            const auto field = std::find_if(
                kBindingFields.begin(), kBindingFields.end(),
                [name](const BindingField& candidate) {
                    return candidate.name == name;
                });
            if (field != kBindingFields.end() && IsValidBinding(*value)) {
                settings.bindings.*(field->member) = *value;
            } else if (field != kBindingFields.end()) {
                shouldRewrite = true;
            }
        }
    }

    // 旧默认迁移到单键布局；其他自定义主键保留，旧备用字段读入时忽略、重写时移除。
    // 新默认被其他自定义动作占用时保留该旧主键，避免升级制造冲突后重置整组。
    // version=4 后自行绑定 C、侧键、Z 或 X 不再触发迁移。
    if (version < 4) {
        const auto migrateDefault = [&](int KeyBindings::*member, int code) {
            for (const auto& field : kBindingFields)
                if (field.member != member && settings.bindings.*field.member == code) return;
            settings.bindings.*member = code;
        };
        if ((version == 1 && settings.bindings.focus == KEY_C) ||
            settings.bindings.focus == MouseBinding(MOUSE_BUTTON_SIDE))
            migrateDefault(&KeyBindings::focus, MouseBinding(MOUSE_BUTTON_RIGHT));
        if (settings.bindings.fire == KEY_Z)
            migrateDefault(&KeyBindings::fire, MouseBinding(MOUSE_BUTTON_LEFT));
        if (settings.bindings.bomb == KEY_X) migrateDefault(&KeyBindings::bomb, KEY_F);
        shouldRewrite = true;
    }
    if(version<6) {
        // 旧 best_score 只记录标准模式一面通关分，可安全迁入标准关卡槽；挑战与其他面不能推断。
        settings.stageHighScores.Record(false,1,settings.bestScore);
        shouldRewrite=true;
    }
    if(settings.spellSelection.group>=spell::Stages()[settings.spellSelection.stage].groups) {
        settings.spellSelection.group=0;shouldRewrite=true;
    }
    if(version<7) {
        settings.spellSelection.character=settings.lastSelection.shot;
        settings.spellSelection.group=settings.lastSelection.practicePhase==4?2:1;
        shouldRewrite=true;
    }
    if (!HasUniqueBindings(settings.bindings)) {
        settings.bindings = KeyBindings{};
        shouldRewrite = true;
    }
    if (shouldRewrite) (void)SaveSettingsFile(path, settings);
    return settings;
}

bool SaveSettingsFile(const std::filesystem::path& path,
                      const GameSettings& settings) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;

    output << "# koumakyou local settings\n";
    output << "version=8\n";
    output << "fps_limit=" << settings.fpsLimit << '\n';
    output << "music_volume=" << settings.musicVolume << '\n';
    output << "mouse_sensitivity=" << settings.mouseSensitivity << '\n';
    output << "sound_volume=" << settings.soundVolume << '\n';
    output << "initial_lives=" << settings.initialLives << '\n';
    output << "initial_bombs=" << settings.initialBombs << '\n';
    output << "best_score=" << settings.bestScore << '\n';
    output << "last_shot=" << settings.lastSelection.shot << '\n';
    output << "last_character=" << settings.lastSelection.character << '\n';
    output << "last_difficulty=" << settings.lastSelection.difficulty << '\n';
    output << "last_challenge=" << settings.lastSelection.challenge << '\n';
    output << "last_practice_phase=" << settings.lastSelection.practicePhase << '\n';
    output << "last_practice_stage=" << settings.lastSelection.practiceStage << '\n';
    for(int mode=0;mode<2;++mode) for(int stage=1;stage<=kStageScoreCount;++stage)
        output << (mode?"stage_best_challenge_":"stage_best_standard_") << stage << '='
               << settings.stageHighScores.scores[mode][stage-1] << '\n';
    for(int difficulty:{0,2,3})for(int mode=0;mode<2;++mode)for(int stage=1;stage<=kStageScoreCount;++stage)
        output << "stage_best_" << difficulty << '_' << (mode?"challenge_":"standard_") << stage << '='
               << settings.stageHighScores.difficultyScores[difficulty][mode][stage-1] << '\n';
    output << "last_spell_character=" << settings.spellSelection.character << '\n';
    output << "last_spell_stage=" << settings.spellSelection.stage << '\n';
    output << "last_spell_group=" << settings.spellSelection.group << '\n';
    output << "last_spell_difficulty=" << settings.spellSelection.difficulty << '\n';
    for(int character=0;character<spell::kCharacterCount;++character)for(int number=1;number<=spell::kRecordCount;++number) {
        const auto& record=settings.spellRecords.Get(character,number);
        if(record.bestScore==0 && record.attempts==0 && record.mainAttempts==0)continue;
        output << "spell_record_" << character << '_' << number << '=' << character << ' ' << number << ' '
               << record.bestScore << ' ' << record.attempts << ' ' << record.captured << ' '
               << record.mainAttempts << ' ' << record.mainCaptured << '\n';
    }
    for (const BindingField& field : kBindingFields) {
        output << field.name << '=' << settings.bindings.*(field.member) << '\n';
    }
    return output.good();
}

}  // namespace koumakyou
