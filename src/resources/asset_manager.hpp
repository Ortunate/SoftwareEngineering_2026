// 本文件负责声明图片、着色器、BGM 和音效的统一资源管理接口。

#pragma once

#include "resources/loaded_assets.hpp"

#include "raylib.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace koumakyou {

/// @brief 主菜单与关卡共同使用、直到程序退出才释放的图形素材。
struct CommonAssets {
    Model skyboxModel{};
    TextureCubemap skyboxTexture{};
    Shader skyboxShader{};
    Texture2D titleBackground{};
    Texture2D reimuPortrait{};
    Texture2D referenceFrame{}, referenceStage1{}; // 管理器独占的原作侧栏图集与一面题签。
    Font uiFont{}; // 管理器持有；缺失时借用 raylib 默认字体，不能重复释放。
};

/// @brief 单个关卡使用的图形素材；由 AssetManager 独占所有权。
struct StageAssets {
    std::vector<VisualResource> visuals{};
    SpriteShaders shaders{};
};

/// @brief 按通用/关卡生命周期统一持有图片、着色器、BGM 和音效。
class AssetManager {
public:
    /// @brief 指定可执行文件旁的素材根目录。
    /// @param root 素材根目录。
    explicit AssetManager(std::filesystem::path root);
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    /// @brief 先释放关卡资源，再释放通用资源。
    ~AssetManager();

    /// @brief 程序启动时加载通用素材；重复调用不重复加载。
    void LoadCommon();
    /// @brief 关卡开始时加载其专属素材；已有的关卡资源会先释放。
    /// @param manifest 由 Stage 内容模块提供的资源清单，仅在调用期间借用。
    void LoadStage(const StageAssetManifest& manifest);
    /// @brief 关卡结束或离开时停止其音乐并释放专属素材。
    void UnloadStage();

    /// @brief 取得不转移所有权的通用图形素材；引用在管理器销毁前有效。
    [[nodiscard]] const CommonAssets& Common() const { return common_; }
    /// @brief 取得不转移所有权的当前关卡图形素材；下次 LoadStage 或 UnloadStage 后失效。
    [[nodiscard]] const StageAssets& Stage() const { return stage_; }
    /// @brief 按当前关卡的外观 ID 查询；无效 ID 返回空资源，不转移纹理所有权。
    /// @return 资源引用；下次 LoadStage 或 UnloadStage 后不得继续使用有效外观引用。
    [[nodiscard]] const VisualResource& GetVisual(VisualId id) const;
    /// @brief 查询当前是否持有关卡专属素材。
    [[nodiscard]] bool HasStage() const { return stageLoaded_; }

    /// @brief 播放通用标题 BGM。
    void PlayMenuMusic();
    /// @brief 播放当前关卡的道中 BGM。
    void PlayStageMusic();
    /// @brief 播放当前关卡的 Boss BGM。
    void PlayBossMusic();
    /// @brief 游戏暂停时暂停当前 BGM。
    void PauseMusic();
    /// @brief 游戏继续时恢复当前 BGM。
    void ResumeMusic();
    /// @brief 每帧更新流式 BGM，维持持续播放。
    void UpdateAudio();
    /// @brief 设置全部已加载/未来加载的 BGM 音量；percent 限定到 0～100，不影响音效。
    void SetMusicVolumePercent(int percent);
    /// @brief 设置全部已加载/未来加载的音效音量；percent 钳制到 0～100，不影响 BGM。
    void SetSoundVolumePercent(int percent);
    /// @brief 按清单中的名字播放通用或当前关卡的音效。
    /// @param name 音效资源名；未配置或同一音效仍在播放时不重启缓冲。
    void PlaySoundEffect(std::string_view name) const;

private:
    enum class MusicSlot : std::uint8_t { None, Menu, Stage, Boss };

    /// @brief 切换 BGM，并在播放新曲之前停止旧曲。
    void SwitchMusic(MusicSlot slot);
    /// @brief 查询音乐槽位对应的流对象。
    [[nodiscard]] Music* MusicFor(MusicSlot slot);
    /// @brief 程序退出时释放通用素材。
    void UnloadCommon();

    std::filesystem::path root_;
    CommonAssets common_{};
    StageAssets stage_{};
    Music menuMusic_{};
    Music stageMusic_{};
    Music bossMusic_{};
    std::unordered_map<std::string, Sound> commonSounds_;
    std::unordered_map<std::string, Sound> stageSounds_;
    std::unordered_map<std::string, Texture2D> stageTextures_;
    MusicSlot activeMusic_ = MusicSlot::None;
    int musicVolumePercent_ = 50;
    int soundVolumePercent_ = 60;
    bool commonLoaded_ = false;
    bool stageLoaded_ = false;
};

}  // namespace koumakyou
