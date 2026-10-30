// 本文件负责实现通用与关卡素材的加载、音频播放和资源卸载。

#include "resources/asset_manager.hpp"
#include "resources/projectile_assets.hpp"
#include "resources/projectile_paths.hpp"
#include "resources/dds_image.hpp"
#include "resources/dialogue_crop.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <chrono>
#include <future>
#include <stdexcept>
#include <unordered_set>
#include <span>
#include <utility>

namespace koumakyou {
namespace {
// 已登记的原生弹种必须随包分发模型；缺包属于加载失败，不能悄悄退回旧外观。
bool NeedsProjectileModels(const StageAssetManifest&manifest){
 for(const auto&v:manifest.visuals){auto a=v.animation.atlas;int script=v.animation.script;
  if(a==battle::Atlas::BeamBullets||(a==battle::Atlas::Bullets&&script>=0&&script<10))return true;
  if(a==battle::Atlas::Reimu&&((script>=64&&script<=66)||(script>=128&&script<=129)||(script>=133&&script<=136)))return true;
  if(a==battle::Atlas::Marisa&&((script>=5&&script<=11)||(script>=64&&script<=71)||(script>=97&&script<=100)||(script>=128&&script<=129)))return true;
 }return false;
}

/// @brief 音效清单项，资源路径相对 assets 根目录。
constexpr std::array<SoundDefinition, 19> kCommonSounds{{
    {"menu_move", "common/se/menu_move.wav"},
    {"menu_confirm", "common/se/menu_confirm.wav"},
    {"menu_cancel", "common/se/menu_cancel.wav"},
    {"shoot", "common/se/shoot.wav"},
    {"enemy_down", "common/se/enemy_down.wav"},
    {"hit", "common/se/hit.wav"},
    {"bomb", "common/se/bomb.wav"},
    {"bomb_master", "common/se/bomb_master.wav"},
    {"graze", "common/se/graze.wav"},
    {"pickup", "common/se/pickup.wav"},
    {"spell", "common/se/spell.wav"},
    {"clear", "common/se/clear.wav"},
    {"full_power", "common/se/full_power.wav"},
    {"reference_6", "common/se/reference_6.wav"},
    {"reference_7", "common/se/reference_7.wav"},
    {"reference_8", "common/se/reference_8.wav"},
    {"reference_9", "common/se/reference_9.wav"},
    {"reference_16", "common/se/reference_16.wav"},
    {"reference_18", "common/se/reference_18.wav"},

}};

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

/// @brief 将自有 UI 字体的 ASCII 数字设为等宽；居中保留原字形，分数更新时不会改变整串宽度。
/// 只调整当前字体的运行时度量，不改 OTF，也不能传入由 raylib 借用的默认字体。
void MakeUiDigitsTabular(Font& font) {
    int advance = 0;
    for (int codepoint = '0'; codepoint <= '9'; ++codepoint)
        advance = std::max(advance, font.glyphs[GetGlyphIndex(font, codepoint)].advanceX);
    for (int codepoint = '0'; codepoint <= '9'; ++codepoint) {
        GlyphInfo& glyph = font.glyphs[GetGlyphIndex(font, codepoint)];
        glyph.offsetX += (advance - glyph.advanceX) / 2;
        glyph.advanceX = advance;
    }
}

/// @brief 按共享 UTF-8 字符表加载 128 像素 UI 字体并设置等宽数字；失败时借用默认字体。
/// 有效自有字体的纹理、字形与度量由调用方负责一起 UnloadFont，不能释放默认字体。
Font LoadUiFont(const std::filesystem::path& path, const std::filesystem::path& charsetPath) {
    if (!FileExists(path.string().c_str())) return GetFontDefault();
    char* charset = LoadFileText(charsetPath.string().c_str());
    if (!charset) return GetFontDefault();
    int count = 0;
    int* codepoints = LoadCodepoints(charset, &count);
    Font font = LoadFontEx(path.string().c_str(), 128, codepoints, count);
    UnloadCodepoints(codepoints);
    UnloadFileText(charset);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    if (font.texture.id != GetFontDefault().texture.id) MakeUiDigitsTabular(font);
    return font;
}

/// @brief 同一路径可以配置不同对话取景，按外观 ID 缓存可见边界；只读取调用方持有的 CPU 图片。
std::unordered_map<VisualId, Rectangle> DialogueCrops(Image image, const std::filesystem::path& path,
                                                  const StageAssetManifest& manifest) {
    std::unordered_map<VisualId, Rectangle> crops;
    for (const auto& definition : manifest.visuals) {
        if (definition.id != kInvalidVisualId && definition.animation.atlas == battle::Atlas::None &&
            definition.path == path.generic_string())
            crops.emplace(definition.id, FindDialogueContent(image, definition.dialogueCrop));
    }
    return crops;
}

}  // namespace

// 每次仅解码一张图片，限制高清图集的 CPU 峰值内存；future 不捕获 AssetManager。
// 线程只读文件并分配 CPU 像素，不接触 OpenGL、音频或运行中的资源缓存。
struct AssetManager::StageLoad {
    struct Item {
        std::filesystem::path path;
        battle::Atlas atlas = battle::Atlas::None;
    };
    struct Decoded {
        Image image{};
        std::shared_ptr<const anm::Archive> archive;
        std::unordered_map<VisualId, Rectangle> dialogueCrops;
        ~Decoded() { if (image.data) UnloadImage(image); }
    };
    StageAssetManifest manifest;
    std::vector<Item> items;
    std::size_t uploaded = 0;
    // 必须先析构：future 等待后台完成，结果的析构器随后释放尚未上传的 CPU 图片。
    std::future<std::unique_ptr<Decoded>> pending;
};

AssetManager::AssetManager(std::filesystem::path root) : root_(std::move(root)) {}

AssetManager::~AssetManager() {
    UnloadStage();
    UnloadCommon();
}

void AssetManager::LoadCommon() {
    if (commonLoaded_) return;
    commonLoaded_ = true;
    common_.titleMoon = LoadTextureAsset(root_ / "common/title_layers/moon.png");
    common_.titleShrine = LoadTextureAsset(root_ / "common/title_layers/shrine.png");
    common_.titleReimu = LoadTextureAsset(root_ / "common/title_layers/reimu.png");
    for (Texture2D* layer : {&common_.titleMoon, &common_.titleShrine, &common_.titleReimu}) {
        if (!IsTextureValid(*layer)) continue;
        GenTextureMipmaps(layer);
        SetTextureFilter(*layer, TEXTURE_FILTER_TRILINEAR);
    }
    // 任一分层素材缺失时整页回退，避免出现只有人物、却没有座下石阶的残缺组合。
    if (!IsTextureValid(common_.titleMoon) || !IsTextureValid(common_.titleShrine) ||
        !IsTextureValid(common_.titleReimu))
        common_.titleBackground = LoadTextureAsset(root_ / "common/title_reimu_modern.png");
    common_.titleLogo = LoadTextureAsset(root_ / "common/title_logo_yidian.png");
    if (IsTextureValid(common_.titleLogo)) {
        // 高分辨率字标会随窗口缩小；mipmap 避免细笔画和装饰线在缩放时跳动、锯齿化。
        GenTextureMipmaps(&common_.titleLogo);
        SetTextureFilter(common_.titleLogo, TEXTURE_FILTER_TRILINEAR);
    }
    common_.reimuPortrait = LoadTextureAsset(root_ / "stages/stage1/portraits/reimu/face00a_1.png");
    common_.marisaBackdrop = LoadTextureAsset(root_ / "common/portraits/marisa/face01a_1.png");
    if(IsTextureValid(common_.marisaBackdrop)) {
        GenTextureMipmaps(&common_.marisaBackdrop);
        SetTextureFilter(common_.marisaBackdrop,TEXTURE_FILTER_TRILINEAR);
    }
    common_.spellPortraits[0]=LoadTextureAsset(root_ / "common/spell_practice/reimu_modern.png");
    common_.spellPortraits[1]=LoadTextureAsset(root_ / "common/spell_practice/marisa_modern.png");
    // 选人全身图在不同窗口中大幅缩小，mipmap 保留发丝、白色花边与纸垂的平滑边缘。
    for(auto& portrait:common_.spellPortraits)if(IsTextureValid(portrait)) {
        GenTextureMipmaps(&portrait);
        SetTextureFilter(portrait,TEXTURE_FILTER_TRILINEAR);
    }
    // 两种字重只生成共享字符表中的字形，独立加载以保持 Bold 的真实笔画和度量。
    const auto charsetPath = root_ / "common/ui_charset.txt";
    common_.uiFont = LoadUiFont(root_ / "common/ui_font.otf", charsetPath);
    common_.hudFont = LoadUiFont(root_ / "common/ui_font_bold.otf", charsetPath);
    if (common_.hudFont.texture.id == GetFontDefault().texture.id) common_.hudFont = common_.uiFont;

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
    SetSoundVolumePercent(soundVolumePercent_);
}

const ProjectileAssets* AssetManager::Projectiles()const{return projectileAssets_&&projectileAssets_->Ready()?projectileAssets_.get():nullptr;}

void AssetManager::LoadStage(const StageAssetManifest& manifest) {
    UnloadStage();
    FinishStageLoad(manifest);
}

void AssetManager::BeginStageLoad(const StageAssetManifest& manifest) {
    UnloadStage();
    if(NeedsProjectileModels(manifest)&&!std::filesystem::exists(projectiles::ProjectileAssetDirectory(root_)))throw std::runtime_error("Projectile model pack is missing");
    stageLoad_ = std::make_unique<StageLoad>();
    stageLoad_->manifest = manifest;
    std::unordered_set<std::string> paths;
    std::unordered_set<battle::Atlas> atlases;
    for (const auto& definition : manifest.visuals) {
        if (definition.id == kInvalidVisualId) continue;
        if (definition.animation.atlas != battle::Atlas::None) {
            if (atlases.insert(definition.animation.atlas).second)
                stageLoad_->items.push_back({{}, definition.animation.atlas});
        } else if (!definition.path.empty() && paths.emplace(definition.path).second) {
            stageLoad_->items.push_back({std::string(definition.path), battle::Atlas::None});
        }
    }
}

bool AssetManager::AdvanceStageLoad() {
    if (!stageLoad_) throw std::logic_error("Stage load has not started");
    auto& load = *stageLoad_;
    if (load.pending.valid()) {
        if (load.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return false;
        auto decoded = load.pending.get(); // 仅在 ready 后取值，异常交给调用方展示失败页。
        const Texture2D texture = LoadTextureFromImage(decoded->image);
        if (!IsTextureValid(texture)) throw std::runtime_error("Cannot upload stage texture");
        const auto& item = load.items[load.uploaded];
        if (item.atlas == battle::Atlas::None) {
            stageTextures_.emplace(item.path.string(), texture);
            stageDialogueCrops_.insert(decoded->dialogueCrops.begin(), decoded->dialogueCrops.end());
        } else {
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
            battleAtlases_.emplace(item.atlas, BattleAtlasResource{texture, std::move(decoded->archive)});
        }
        ++load.uploaded;
        // 包括最后一张纹理在内，上传后先返还一帧；模型、音频及其余着色器绑定留到后续加载步骤。
        return false;
    }
    if (load.uploaded == load.items.size()) {
        if(std::filesystem::exists(projectiles::ProjectileAssetDirectory(root_))){
            if(!projectileAssets_)projectileAssets_=std::make_unique<ProjectileAssets>(root_,false);
            if(!projectileAssets_->AdvanceLoad())return false;
        }
        FinishStageLoad(load.manifest);
        stageLoad_.reset();
        return true;
    }
    const auto item = load.items[load.uploaded];
    const auto root = root_;
    const auto manifest = load.manifest; // 清单字符串覆盖整个异步加载期，后台不捕获管理器本身。
    load.pending = std::async(std::launch::async, [root, item, manifest] {
        auto decoded = std::make_unique<StageLoad::Decoded>();
        if (item.atlas == battle::Atlas::None) {
            decoded->image = LoadImage((root / item.path).string().c_str());
            decoded->dialogueCrops = DialogueCrops(decoded->image, item.path, manifest);
        } else {
            const auto& entry = battle::Describe(item.atlas);
            decoded->archive = std::make_shared<anm::Archive>(anm::LoadArchive(root / entry.anm));
            decoded->image = LoadOriginalDds(root / entry.dds);
        }
        if (!IsImageValid(decoded->image)) throw std::runtime_error("Cannot decode stage texture");
        return decoded;
    });
    return false;
}

float AssetManager::StageLoadProgress() const {
    if (!stageLoad_) return stageLoaded_ ? 1.0F : 0.0F;
    const float modelSteps=std::filesystem::exists(projectiles::ProjectileAssetDirectory(root_))?float(ProjectileAssets::LoadSteps):0.F;
    const float modelDone=projectileAssets_?projectileAssets_->LoadProgress()*modelSteps:0.F;
    return (stageLoad_->uploaded+modelDone)/(stageLoad_->items.size()+1+modelSteps);
}

void AssetManager::FinishStageLoad(const StageAssetManifest& manifest) {
    if(NeedsProjectileModels(manifest)&&!std::filesystem::exists(projectiles::ProjectileAssetDirectory(root_)))throw std::runtime_error("Projectile model pack is missing");
    if(!projectileAssets_&&std::filesystem::exists(projectiles::ProjectileAssetDirectory(root_)))projectileAssets_=std::make_unique<ProjectileAssets>(root_);
    for (const VisualDefinition& definition : manifest.visuals) {
        if (definition.id == kInvalidVisualId) continue;
        if (stage_.visuals.size() <= definition.id)
            stage_.visuals.resize(static_cast<std::size_t>(definition.id) + 1);
        Texture2D texture{};
        if (definition.animation.atlas==battle::Atlas::None && !definition.path.empty()) {
            const std::string path(definition.path);
            // 多个外观可以共用一张贴图，实际所有权由路径缓存统一管理。
            auto [it, inserted] = stageTextures_.try_emplace(path);
            if (inserted) {
                Image image = LoadImage((root_ / path).string().c_str());
                it->second = LoadTextureFromImage(image);
                const auto crops = DialogueCrops(image, path, manifest);
                stageDialogueCrops_.insert(crops.begin(), crops.end());
                if (image.data) UnloadImage(image);
            }
            texture = it->second;
        }
        stage_.visuals[definition.id] = {texture, definition.mode,
            definition.worldHeight, definition.pivot,
            definition.bulletDiameterScale, definition.glow, definition.dialogueCrop, {}, definition.defeatVisual, {}};
        if (const auto crop = stageDialogueCrops_.find(definition.id); crop != stageDialogueCrops_.end())
            stage_.visuals[definition.id].dialogueCrop = crop->second;
        if(definition.animation.atlas!=battle::Atlas::None) {
            const auto& atlas=LoadBattleAtlas(definition.animation.atlas);
            auto& visual=stage_.visuals[definition.id];
            visual.texture=atlas.texture;visual.archive=atlas.archive;visual.animation=definition.animation;
            if(!visual.archive->FindScript(visual.animation.script))
                throw std::runtime_error("Visual references missing native animation");
        }
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
    SetSoundVolumePercent(soundVolumePercent_);
    stageLoaded_ = true;
}

const VisualResource& AssetManager::GetVisual(VisualId id) const {
    static const VisualResource empty{};
    return id < stage_.visuals.size() ? stage_.visuals[id] : empty;
}

const BattleAtlasResource& AssetManager::LoadBattleAtlas(battle::Atlas id) {
    if(const auto it=battleAtlases_.find(id);it!=battleAtlases_.end())return it->second;
    const auto& entry=battle::Describe(id);
    auto archive=std::make_shared<anm::Archive>(anm::LoadArchive(root_/entry.anm));
    Image image=LoadOriginalDds(root_/entry.dds);
    const Texture2D texture=LoadTextureFromImage(image);UnloadImage(image);
    if(!IsTextureValid(texture))throw std::runtime_error("Cannot upload native DDS");
    SetTextureFilter(texture,TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(texture,TEXTURE_WRAP_REPEAT); // 原 ANM 特效的 UV 滚动/越界区域使用重复采样。
    return battleAtlases_.emplace(id,BattleAtlasResource{texture,std::move(archive)}).first->second;
}

void AssetManager::UnloadStage() {
    projectileAssets_.reset();
    // 等待唯一的在途 CPU 解码；随后才能释放清单字符串或销毁图形上下文。
    stageLoad_.reset();
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
    stageDialogueCrops_.clear();
    for(auto& [id,atlas]:battleAtlases_) { (void)id;UnloadTextureAsset(atlas.texture); }
    battleAtlases_.clear();
    stage_ = {};
    stageLoaded_ = false;
}

void AssetManager::UnloadCommon() {
    if (!commonLoaded_) return;
    SwitchMusic(MusicSlot::None);
    UnloadSounds(commonSounds_);
    UnloadMusicAsset(menuMusic_);
    UnloadTextureAsset(common_.titleBackground);
    UnloadTextureAsset(common_.titleMoon);
    UnloadTextureAsset(common_.titleShrine);
    UnloadTextureAsset(common_.titleReimu);
    UnloadTextureAsset(common_.titleLogo);
    UnloadTextureAsset(common_.reimuPortrait);
    UnloadTextureAsset(common_.marisaBackdrop);
    for(auto& portrait:common_.spellPortraits)UnloadTextureAsset(portrait);
    if (common_.hudFont.texture.id != 0 && common_.hudFont.texture.id != common_.uiFont.texture.id &&
        common_.hudFont.texture.id != GetFontDefault().texture.id)
        UnloadFont(common_.hudFont);
    if (common_.uiFont.texture.id != 0 &&
        common_.uiFont.texture.id != GetFontDefault().texture.id)
        UnloadFont(common_.uiFont);
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

void AssetManager::SeekMusic(float seconds) {
    if(Music* music=MusicFor(activeMusic_);music && IsMusicValid(*music)) {
        const float length=GetMusicTimeLength(*music);if(length>0)SeekMusicStream(*music,std::fmod(std::max(0.0F,seconds),length));
    }
}
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

void AssetManager::SetSoundVolumePercent(int percent) {
    soundVolumePercent_ = std::clamp(percent, 0, 100);
    for (const auto* bank : {&commonSounds_, &stageSounds_}) {
        for (const auto& [name, sound] : *bank) {
            (void)name;
            SetSoundVolume(sound, soundVolumePercent_ / 100.0F);
        }
    }
}

void AssetManager::PlaySoundEffect(std::string_view name) const {
    if (!IsAudioDeviceReady()) return;
    const std::string key(name);
    if (const auto stageSound = stageSounds_.find(key);
        stageSound != stageSounds_.end()) {
        if (!IsSoundPlaying(stageSound->second)) ::PlaySound(stageSound->second);
    } else if (const auto commonSound = commonSounds_.find(key);
               commonSound != commonSounds_.end()) {
        // 同一帧的多个击破/拾取共用一个音效，不能不断重启缓冲产生爆音。
        if (!IsSoundPlaying(commonSound->second)) ::PlaySound(commonSound->second);
    }
}
const Sound* AssetManager::FindSoundEffect(std::string_view name) const {
    if(!IsAudioDeviceReady())return nullptr;
    const std::string key(name);
    if(const auto found=stageSounds_.find(key);found!=stageSounds_.end())return &found->second;
    if(const auto found=commonSounds_.find(key);found!=commonSounds_.end())return &found->second;
    return nullptr;
}
bool AssetManager::PauseSoundEffect(std::string_view name) const {
    const auto* sound=FindSoundEffect(name);
    if(!sound || !IsSoundPlaying(*sound))return false;
    ::PauseSound(*sound);return true;
}
void AssetManager::ResumeSoundEffect(std::string_view name) const {
    if(const auto* sound=FindSoundEffect(name))::ResumeSound(*sound);
}
void AssetManager::StopSoundEffect(std::string_view name) const {
    if(const auto* sound=FindSoundEffect(name))::StopSound(*sound);
}

}  // namespace koumakyou
