// 本文件负责实现通用与关卡素材的加载、音频播放和资源卸载。

#include "resources/asset_manager.hpp"

#include "rlgl.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace redmist {
namespace {

/// @brief 音效清单项，资源路径相对 assets 根目录。
// 目前素材包没有音效文件；后续只需在清单中登记，不必修改加载与卸载流程。
constexpr std::array<SoundDefinition, 0> kCommonSounds{};

/// @brief 读取一张贴图；调用方负责在对应生命周期结束时释放。
/// @param path 图片文件路径。
/// @return 已加载的纹理句柄。
Texture2D LoadTextureAsset(const std::filesystem::path& path) {
    return LoadTexture(path.string().c_str());
}

/// @brief 读取可选的着色器；资源缺失时由绘制器使用默认渲染。
/// @param vertex 顶点着色器路径；为空时使用 raylib 默认顶点着色器。
/// @param fragment 片元着色器路径。
/// @return 有效着色器句柄，资源缺失时为空。
Shader LoadShaderAsset(const std::filesystem::path& vertex,
                       const std::filesystem::path& fragment) {
    if (!FileExists(fragment.string().c_str())) return {};
    if (!vertex.empty() && !FileExists(vertex.string().c_str())) return {};
    return LoadShader(vertex.empty() ? nullptr : vertex.string().c_str(),
                      fragment.string().c_str());
}

/// @brief 仅在音频设备已初始化时打开流式 BGM。
/// @param path 音乐文件路径。
/// @return 可持续更新的音乐流，音频不可用时为空。
Music LoadMusicAsset(const std::filesystem::path& path) {
    if (!IsAudioDeviceReady() || !FileExists(path.string().c_str())) return {};
    Music music = LoadMusicStream(path.string().c_str());
    if (IsMusicValid(music)) {
        music.looping = true;
        SetMusicVolume(music, 0.5F);
    }
    return music;
}

/// @brief 将一组音效按名字加载到所属生命周期的资源表。
/// @param bank 目标音效资源表。
/// @param root 素材根目录。
/// @param entries 需要加载的音效清单。
void LoadSounds(std::unordered_map<std::string, Sound>& bank,
                const std::filesystem::path& root,
                std::span<const SoundDefinition> entries) {
    if (!IsAudioDeviceReady()) return;
    for (const SoundDefinition& entry : entries) {
        const std::filesystem::path path = root / entry.path;
        if (!FileExists(path.string().c_str())) continue;
        Sound sound = LoadSound(path.string().c_str());
        if (IsSoundValid(sound)) {
            bank.emplace(entry.name, sound);
        }
    }
}

/// @brief 卸载一个音效资源表中的所有音频缓冲。
/// @param bank 要清空的音效资源表。
void UnloadSounds(std::unordered_map<std::string, Sound>& bank) {
    for (const auto& [name, sound] : bank) {
        (void)name;
        if (IsSoundValid(sound)) UnloadSound(sound);
    }
    bank.clear();
}

/// @brief 对 raylib 可能返回的默认着色器避免重复释放。
/// @param shader 要释放并清空的着色器句柄。
void UnloadShaderAsset(Shader& shader) {
    if (shader.id != 0 && shader.id != rlGetShaderIdDefault()) {
        UnloadShader(shader);
    }
    shader = {};
}

/// @brief 释放一张有效贴图并清空句柄。
/// @param texture 要释放的纹理句柄。
void UnloadTextureAsset(Texture2D& texture) {
    if (IsTextureValid(texture)) UnloadTexture(texture);
    texture = {};
}

/// @brief 停止并关闭一条有效的音乐流。
/// @param music 要关闭的音乐流。
void UnloadMusicAsset(Music& music) {
    if (IsMusicValid(music)) {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
    music = {};
}

}  // namespace

AssetManager::AssetManager(std::filesystem::path root) : root_(std::move(root)) {}

AssetManager::~AssetManager() {
    UnloadStage();
    UnloadCommon();
}

void AssetManager::LoadCommon() {
    if (commonLoaded_) return;
    commonLoaded_ = true;

    const std::filesystem::path skyboxPath = root_ / "common/crimson_mist_skybox.png";
    Image skyboxImage = LoadImage(skyboxPath.string().c_str());
    if (skyboxImage.data != nullptr &&
        skyboxImage.width == skyboxImage.height * 6) {
        common_.skyboxTexture = LoadTextureCubemap(
            skyboxImage, CUBEMAP_LAYOUT_LINE_HORIZONTAL);
    }
    if (skyboxImage.data != nullptr) UnloadImage(skyboxImage);
    if (IsTextureValid(common_.skyboxTexture)) {
        const std::filesystem::path shaders = root_ / "common/shaders";
        common_.skyboxShader = LoadShaderAsset(shaders / "skybox.vs",
                                                shaders / "skybox.fs");
        if (IsShaderValid(common_.skyboxShader) &&
            common_.skyboxShader.id != rlGetShaderIdDefault()) {
            const int environmentLocation =
                GetShaderLocation(common_.skyboxShader, "environmentMap");
            if (environmentLocation >= 0) {
                const int textureUnit = MATERIAL_MAP_CUBEMAP;
                SetShaderValue(common_.skyboxShader, environmentLocation,
                               &textureUnit, SHADER_UNIFORM_INT);
                // Model 材质借用着色器和立方体贴图；卸载时须先释放 Model。
                common_.skyboxModel = LoadModelFromMesh(
                    GenMeshCube(1.0F, 1.0F, 1.0F));
                common_.skyboxModel.materials[0].shader = common_.skyboxShader;
                common_.skyboxModel.materials[0]
                    .maps[MATERIAL_MAP_CUBEMAP].texture = common_.skyboxTexture;
            }
        }
    }

    menuMusic_ = LoadMusicAsset(root_ / "common" /
                                "比赤色更鲜红的梦.mp3");
    SetMusicVolumePercent(musicVolumePercent_);
    LoadSounds(commonSounds_, root_, kCommonSounds);
}

void AssetManager::LoadStage(const StageAssetManifest& manifest) {
    UnloadStage();
    for (const VisualDefinition& definition : manifest.visuals) {
        if (definition.id == kInvalidVisualId) continue;
        if (stage_.visuals.size() <= definition.id)
            stage_.visuals.resize(static_cast<std::size_t>(definition.id) + 1);
        Texture2D texture{};
        if (!definition.path.empty()) {
            const std::string path(definition.path);
            // 多个外观可以共用一张贴图，实际所有权由路径缓存统一管理。
            auto [it, inserted] = stageTextures_.try_emplace(path);
            if (inserted) it->second = LoadTextureAsset(root_ / path);
            texture = it->second;
        }
        stage_.visuals[definition.id] = {texture, definition.mode,
            definition.worldHeight, definition.pivot,
            definition.bulletDiameterScale, definition.glow};
    }
    const std::filesystem::path shaders = root_ / "common/shaders";
    stage_.shaders.bulletCore = LoadShaderAsset({}, shaders / "bullet_core.fs");
    stage_.shaders.cutout = LoadShaderAsset({}, shaders / "sprite_cutout.fs");
    if (!manifest.stageBgm.empty())
        stageMusic_ = LoadMusicAsset(root_ / std::string(manifest.stageBgm));
    if (!manifest.bossBgm.empty())
        bossMusic_ = LoadMusicAsset(root_ / std::string(manifest.bossBgm));
    SetMusicVolumePercent(musicVolumePercent_);
    LoadSounds(stageSounds_, root_, manifest.sounds);
    stageLoaded_ = true;
}

const VisualResource& AssetManager::GetVisual(VisualId id) const {
    static const VisualResource empty{};
    return id < stage_.visuals.size() ? stage_.visuals[id] : empty;
}

void AssetManager::UnloadStage() {
    if (activeMusic_ == MusicSlot::Stage || activeMusic_ == MusicSlot::Boss) {
        SwitchMusic(MusicSlot::None);
    }
    UnloadSounds(stageSounds_);
    UnloadMusicAsset(stageMusic_);
    UnloadMusicAsset(bossMusic_);
    UnloadShaderAsset(stage_.shaders.bulletCore);
    UnloadShaderAsset(stage_.shaders.cutout);
    for (auto& [path, texture] : stageTextures_) {
        (void)path;
        UnloadTextureAsset(texture);
    }
    stageTextures_.clear();
    stage_ = {};
    stageLoaded_ = false;
}

void AssetManager::UnloadCommon() {
    if (!commonLoaded_) return;
    SwitchMusic(MusicSlot::None);
    UnloadSounds(commonSounds_);
    UnloadMusicAsset(menuMusic_);
    if (common_.skyboxModel.meshCount != 0) UnloadModel(common_.skyboxModel);
    UnloadShaderAsset(common_.skyboxShader);
    if (IsTextureValid(common_.skyboxTexture)) UnloadTexture(common_.skyboxTexture);
    common_ = {};
    commonLoaded_ = false;
}

Music* AssetManager::MusicFor(MusicSlot slot) {
    switch (slot) {
        case MusicSlot::Menu: return &menuMusic_;
        case MusicSlot::Stage: return &stageMusic_;
        case MusicSlot::Boss: return &bossMusic_;
        case MusicSlot::None: return nullptr;
    }
    return nullptr;
}

void AssetManager::SwitchMusic(MusicSlot slot) {
    if (slot == activeMusic_) return;
    if (Music* current = MusicFor(activeMusic_);
        current != nullptr && IsMusicValid(*current)) {
        StopMusicStream(*current);
    }
    activeMusic_ = MusicSlot::None;
    if (Music* next = MusicFor(slot); next != nullptr && IsMusicValid(*next)) {
        PlayMusicStream(*next);
        activeMusic_ = slot;
    }
}

void AssetManager::PlayMenuMusic() { SwitchMusic(MusicSlot::Menu); }
void AssetManager::PlayStageMusic() { SwitchMusic(MusicSlot::Stage); }
void AssetManager::PlayBossMusic() { SwitchMusic(MusicSlot::Boss); }

void AssetManager::PauseMusic() {
    if (Music* music = MusicFor(activeMusic_);
        music != nullptr && IsMusicValid(*music)) {
        PauseMusicStream(*music);
    }
}

void AssetManager::ResumeMusic() {
    if (Music* music = MusicFor(activeMusic_);
        music != nullptr && IsMusicValid(*music)) {
        ResumeMusicStream(*music);
    }
}

void AssetManager::SetMusicVolumePercent(int percent) {
    musicVolumePercent_ = std::clamp(percent, 0, 100);
    for (Music* music : {&menuMusic_, &stageMusic_, &bossMusic_}) {
        if (IsMusicValid(*music)) SetMusicVolume(*music, musicVolumePercent_ / 100.0F);
    }
}

void AssetManager::UpdateAudio() {
    if (Music* music = MusicFor(activeMusic_);
        music != nullptr && IsMusicValid(*music)) {
        UpdateMusicStream(*music);
    }
}

void AssetManager::PlaySoundEffect(std::string_view name) const {
    if (!IsAudioDeviceReady()) return;
    const std::string key(name);
    if (const auto stageSound = stageSounds_.find(key);
        stageSound != stageSounds_.end()) {
        ::PlaySound(stageSound->second);
    } else if (const auto commonSound = commonSounds_.find(key);
               commonSound != commonSounds_.end()) {
        ::PlaySound(commonSound->second);
    }
}

}  // namespace redmist
