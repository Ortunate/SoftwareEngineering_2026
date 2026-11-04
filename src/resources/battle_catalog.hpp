// 本文件登记新典全部战斗图集，并提供不依赖当前关卡的原始动画选择接口。
#pragma once
#include "resources/animation.hpp"
#include <cstdint>
#include <span>
#include <string_view>

namespace koumakyou::battle {
/// @brief 稳定图集编号；与 Stage 的 VisualId、ANM 精灵编号和脚本编号相互独立。
enum class Atlas : std::uint8_t {
    Bullets, BeamBullets, BulletEffects, Reimu, Marisa, Effects,
    Stage1Enemies, Rumia, Stage2Enemies, Cirno, Stage3Enemies, Stage4Enemies,
    Stage5Enemies, Sakuya, Stage6Enemies, Remilia, ExtraEnemies, Flandre,
    Stage1Effects, Stage2Effects, Stage3Effects, Stage4Effects, Stage5Effects, Stage6Effects, ExtraEffects,
    Count, None = 255
};
/// @brief 按文件名定位原始 DDS/ANM；paths 相对运行时根目录，所有字符串具有静态生命周期。
struct Definition { Atlas id; std::string_view name, dds, anm; int stage; };
/// @brief 不持有资源的动画选择；script/spriteOffset/initialSprite 均属于所选图集的编号范围。
/// initialSprite=-1 时由脚本选择；无 setSprite 的特效脚本须由调用方指定初始精灵。
struct Selection {
    Atlas atlas = Atlas::None;
    int script = 0, spriteOffset = 0, initialSprite = -1;
    bool operator==(const Selection&) const = default;
};
/// @brief 返回全部 25 组素材元数据；登记不触发磁盘读取或 GPU 加载。
[[nodiscard]] std::span<const Definition> Catalog();
/// @brief 按稳定编号查询，非法编号抛 invalid_argument。
[[nodiscard]] const Definition& Describe(Atlas id);
/// @brief 原作弹幕类型 0–9 及颜色 0–15，取对应脚本和图集色差；无效输入抛 invalid_argument。
/// 大玉/刀弹使用 8 色，火球仅一色，大泡使用 5 色；超出该类型色数的颜色循环到有效色。
[[nodiscard]] Selection EnemyBullet(int type, int color);
/// @brief 返回弹种 0～9、颜色 0～15 的逻辑宽/高，单位为世界长度；非法编号抛 invalid_argument。
/// 这是新典 ANM 加载后的几何尺寸，独立于裁图像素和高清 DDS；反弹使用半尺寸，不能从视觉贴图反推。
[[nodiscard]] Vector2 EnemyBulletLogicalSize(int type,int color=0);
/// @brief 返回新典普通敌弹的世界判定半径；模板不随颜色变化，非法弹种抛 invalid_argument。
[[nodiscard]] float EnemyBulletHitRadius(int type);
/// @brief 原显现动画：frames 必须为 10/16/32；颜色按原 16/8 色映射到显现光圈色。
[[nodiscard]] Selection BulletSpawn(int type, int color, int frames);
/// @brief 原道具编号 0–6，包含未使用的 Bomb、满火力和残机；无效输入抛 invalid_argument。
[[nodiscard]] Selection Item(int type);
/// @brief 角色动作 0–4 为待机、左移、左收势、右移、右收势，可直接绑定未来自机演员。
[[nodiscard]] Selection Player(Atlas character, int action);
/// @brief 同一演员当前动画内的一次中断；age 为该动画开始后的秒数，revision/id 排除已替换动画和重复交付。
struct AnimationInterrupt {double age=0;int label=0;std::uint32_t revision=0;std::uint64_t id=0;};
/// @brief 随演员/弹幕保存的动画游标；借用 CPU 档案，卸载后只能重置，不能继续推进旧实例。
struct Cursor {
    anm::Player player;
    Selection selection{};
    std::uint32_t revision = 0;
    double time = 0;
    std::uint64_t appliedInterrupt=0; // 只属于当前脚本启动序列，重启或复用游标时归零。
    /// @brief age 为本次脚本启动后的非负秒数；同龄重复调用不推进，回退或 revision 改变时重启。
    /// 没有外部动作切换的对象可用出生年龄；不会改变调用方的判定或生命周期。
    /// interrupt 只在本次调用借用，年龄须位于 [0,age]、id 须为正；同一 id 只交付一次，旧 revision 不影响新动画。
    void Sync(const anm::Archive& archive, Selection selected, double age, std::uint32_t serial = 0,const AnimationInterrupt* interrupt=nullptr);
};
} // namespace koumakyou::battle
