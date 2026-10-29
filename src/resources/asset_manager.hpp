// 本文件负责声明模型、图片、着色器、BGM 和音效的统一资源管理接口。

#pragma once

#include "resources/loaded_assets.hpp"

#include "raylib.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace koumakyou {
class ProjectileAssets;

/// @brief 主菜单与关卡共同使用、直到程序退出才释放的图形素材。
struct CommonAssets {
    Model skyboxModel{};
    TextureCubemap skyboxTexture{};
    Shader skyboxShader{};
    Texture2D titleBackground{}; // 三图层缺失时才加载的旧合成背景，由管理器独占。
    Texture2D titleMoon{}, titleShrine{}, titleReimu{}; // 首页独立透明图层；分别持有纹理，不与关卡立绘共用句柄。
    Texture2D titleLogo{}; // 管理器独占的透明首页字标，随通用素材加载/释放；无效时绘制文字替代。
    Texture2D reimuPortrait{}; // 开局选择页保留的灵梦后景图，独立持有并随通用素材卸载。
    Texture2D marisaBackdrop{}; // 魔理沙提围裙姿势的现代重绘后景；不同于骑扫帚前景，独立持有。
    std::array<Texture2D,2> spellPortraits{}; // 现代重绘的自机选人立绘，0 灵梦/1 魔理沙，通用资源独占并统一释放。
    Font uiFont{}; // 菜单等界面的 SemiBold 衬线字体与等宽数字；管理器持有，缺失时借用默认字体。
    Font hudFont{}; // 战斗浮窗的 Bold 字体；独立持有，缺失时借用 uiFont，卸载时不能重复释放。
};

/// @brief 单个关卡使用的图形素材；由 AssetManager 独占所有权。
struct StageAssets {
    std::vector<VisualResource> visuals{};
    SpriteShaders shaders{};
};

/// @brief 按通用/关卡生命周期统一持有模型、图片、着色器、BGM 和音效；所有接口由图形主线程调用。
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
    /// @brief 开始后台读取与解码；GPU/音频操作仍由主线程的 AdvanceStageLoad 完成。
    /// 清单及其字符串须保持有效，直到加载完成或 UnloadStage；不能与 LoadStage 交错调用。
    void BeginStageLoad(const StageAssetManifest& manifest);
    /// @brief 主线程每帧调用一次，最多上传一张原图集纹理或加载一款模型；完成后返回 true，读取/上传失败抛异常。
    /// 首次调用启动后台任务，调用方应先显示加载页；未开始任务时调用属于逻辑错误。
    [[nodiscard]] bool AdvanceStageLoad();
    /// @brief 已上传资源占比（0～1），仅用于界面反馈；后台解码期间保持当前实际进度。
    [[nodiscard]] float StageLoadProgress() const;
    /// @brief 关卡结束或离开时停止其音乐并释放专属素材。
    void UnloadStage();

    /// @brief 取得不转移所有权的通用图形素材；引用在管理器销毁前有效。
    [[nodiscard]] const CommonAssets& Common() const { return common_; }
    /// @brief 取得不转移所有权的当前关卡图形素材；下次 LoadStage 或 UnloadStage 后失效。
    [[nodiscard]] const StageAssets& Stage() const { return stage_; }
    /// @brief 按当前关卡的外观 ID 查询；无效 ID 返回空资源，不转移纹理所有权。
    /// @return 资源引用；下次 LoadStage 或 UnloadStage 后不得继续使用有效外观引用。
    [[nodiscard]] const VisualResource& GetVisual(VisualId id) const;
    /// @brief 按目录编号加载原始 DDS/ANM，重复调用复用纹理；需有效图形上下文。
    /// 不依赖当前关卡，可供未来自机/关卡或素材浏览器使用；缺失/损坏资源抛异常。
    /// 返回借用引用，随 UnloadStage 或管理器销毁失效，调用方不得释放纹理。
    [[nodiscard]] const BattleAtlasResource& LoadBattleAtlas(battle::Atlas id);
    /// @brief 借用已加载的模型资源；无模型素材的测试关卡返回空，不转移所有权。
    const ProjectileAssets* Projectiles() const;
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
    /// @brief 暂停正在播放的长音效并返回是否实际暂停；无音频设备、未配置或未播放返回 false。
    [[nodiscard]] bool PauseSoundEffect(std::string_view name) const;
    /// @brief 恢复先前成功暂停的音效；调用方负责仅对实际暂停项调用，资源缺失时忽略。
    void ResumeSoundEffect(std::string_view name) const;
    /// @brief 取消指定音效并复位，供长演出退出使用；无设备或资源时忽略。
    void StopSoundEffect(std::string_view name) const;

private:
    /// @brief 优先查询关卡音效，借用指针仅在对应资源表有效期内使用；无设备或资源返回空。
    [[nodiscard]] const Sound* FindSoundEffect(std::string_view name) const;
    struct StageLoad; // 独占后台任务与 CPU 图片；取消/析构时等待当前解码完成，再释放结果。
    /// @brief 在纹理缓存已备好后绑定外观、着色器与音频；只允许在主线程调用。
    void FinishStageLoad(const StageAssetManifest& manifest);
    enum class MusicSlot : std::uint8_t { None, Menu, Stage, Boss };

    /// @brief 切换 BGM，并在播放新曲之前停止旧曲。
    void SwitchMusic(MusicSlot slot);
    /// @brief 查询音乐槽位对应的流对象。
    [[nodiscard]] Music* MusicFor(MusicSlot slot);
    /// @brief 程序退出时释放通用素材。
    void UnloadCommon();

    std::filesystem::path root_;
    std::unique_ptr<ProjectileAssets> projectileAssets_;
    CommonAssets common_{};
    StageAssets stage_{};
    std::unique_ptr<StageLoad> stageLoad_;
    Music menuMusic_{};
    Music stageMusic_{};
    Music bossMusic_{};
    std::unordered_map<std::string, Sound> commonSounds_;
    std::unordered_map<std::string, Sound> stageSounds_;
    std::unordered_map<std::string, Texture2D> stageTextures_;
    std::unordered_map<VisualId, Rectangle> stageDialogueCrops_; // CPU 解码时计算的可见取景，随关卡清空。
    std::unordered_map<battle::Atlas, BattleAtlasResource> battleAtlases_; // 按需图集统一随关卡卸载，目录本身不占 GPU。
    MusicSlot activeMusic_ = MusicSlot::None;
    int musicVolumePercent_ = 50;
    int soundVolumePercent_ = 60;
    bool commonLoaded_ = false;
    bool stageLoaded_ = false;
};

}  // namespace koumakyou
