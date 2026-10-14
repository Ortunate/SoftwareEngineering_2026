// 本文件定义原始 ANM 的只读指令数据及独立的 60 Hz 动画实例，不持有 GPU 纹理。
#pragma once
#include "raylib.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace koumakyou::anm {
/// @brief 精灵编号仅在所属图集内有效；source 使用 ANM 逻辑像素，允许特效越过图集边界。
struct Sprite { int id = -1; Rectangle source{}; };
/// @brief 原指令时间以 60 Hz 帧计；args 保存小端原始位，不把整数和浮点互相转换。
struct Instruction {
    int frame = 0, opcode = 0;
    std::uint32_t offset = 0;
    std::array<std::uint32_t, 4> args{};
    [[nodiscard]] int Int(int index) const;
    [[nodiscard]] float Float(int index) const;
};
/// @brief 指令偏移相对本脚本起点；外部中断标签不等同于脚本编号。
struct Script { int id = -1; std::vector<Instruction> instructions; };
/// @brief 不可变的图集元数据；由 AssetManager 持有，实例借用期间不得销毁或替换。
struct Archive {
    int width = 0, height = 0;
    std::vector<Sprite> sprites;
    std::vector<Script> scripts;
    [[nodiscard]] const Sprite* FindSprite(int id) const;
    [[nodiscard]] const Script* FindScript(int id) const;
};
/// @brief 读取新典单条目、版本 0 的外置纹理 ANM；损坏偏移、参数或未知操作码抛 runtime_error。
[[nodiscard]] Archive LoadArchive(const std::filesystem::path& path);
/// @brief 仅含值的当前动画帧；角度为弧度，position 为图集逻辑像素，uvScroll 为整张纹理比例。
/// positionOffset 独立于 position；原 positionMode 切换只改变后续位移指令的目标，不丢弃旧通道。
/// additive/disableZWrite 为混合及深度写入请求；autoRotate 由调用方提供运动方向。
struct Frame {
    int sprite = -1;
    Rectangle source{};
    Vector2 scale{1, 1}, uvScroll{};
    Vector3 rotation{}, position{}, positionOffset{};
    Color color = WHITE;
    bool visible = false, additive = false, topLeft = false, disableZWrite = false;
    int autoRotate = 0;
};
/// @brief 每个演员/弹幕独占一实例；脚本与档案仅借用，暂停时不调用 Advance。
class Player {
public:
    /// @brief 从第 0 帧启动脚本；spriteOffset 用于原作弹幕颜色，initialSprite 用于外部选择的粒子。
    /// seed 仅影响 spriteRand，可重放；未知脚本/精灵、无进展循环会抛异常。
    void Start(const Archive& archive, int script, int spriteOffset = 0,
               int initialSprite = -1, std::uint32_t seed = 1);
    /// @brief 按非负有限秒数补算固定 60 Hz；停止指令冻结脚本时间，插值仍继续直到 static/delete。
    void Advance(double seconds);
    /// @brief 触发原中断标签，找不到标签时使用 -1 备用标签；无匹配返回 false，不改变当前帧。
    bool Interrupt(int label);
    /// @brief 替换当前精灵，不重启脚本，供原作激光/特效的外部控制使用。
    void SetSprite(int id);
    [[nodiscard]] const Frame& Sample() const { return frame_; }
    [[nodiscard]] bool Finished() const { return finished_; }
    [[nodiscard]] int ScriptId() const { return script_ ? script_->id : -1; }
    [[nodiscard]] const Archive* Source() const { return archive_; }
private:
    void Tick();
    const Archive* archive_ = nullptr;
    const Script* script_ = nullptr;
    Frame frame_{};
    std::size_t pc_ = 0;
    int clock_ = 0, spriteOffset_ = 0;
    double remainder_ = 0;
    std::uint32_t random_ = 1;
    bool halted_ = false, finished_ = true, positionOffset_ = false, flipX_ = false, flipY_ = false;
    Vector3 rotationSpeed_{}, positionStart_{}, positionTarget_{};
    Vector2 scaleSpeed_{}, scaleStart_{}, scaleTarget_{};
    int scaleDuration_ = 0, scaleTime_ = 0, alphaDuration_ = 0, alphaTime_ = 0;
    int alphaStart_ = 255, alphaTarget_ = 255;
    int positionDuration_ = 0, positionTime_ = 0, positionMode_ = 0;
};
} // namespace koumakyou::anm
