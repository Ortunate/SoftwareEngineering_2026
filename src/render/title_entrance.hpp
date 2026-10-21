// 本文件保存首页启动时五段依次进场的展示时钟，并提供菜单解锁状态，不推进关卡时间轴。
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
/// @brief 首页启动顺序；仅表示开始时间，实际绘制仍按远近顺序合成。
enum class TitleElement { Moon, Title, Shrine, Reimu, Menu };

/// @brief 一次性的首页启动展示；时间单位为秒，Draw 只读取，离开首页后完成而不在返回时重播。
class TitleEntrance {
public:
    /// @brief 推进非负秒数；非法时间不推进，最长保留到菜单全部显示。
    void Advance(float seconds) {
        if (std::isfinite(seconds)) elapsed_ = std::min(boundaries_.back(), elapsed_ + std::max(0.0F,seconds));
    }
    /// @brief 完成所有图层，供离开首页时调用；返回首页无需再等待启动动画。
    void Complete() { elapsed_ = boundaries_.back(); }
    /// @brief 指定图层的缓出进度（0～1）；前一层完全显示后下一层才开始。
    [[nodiscard]] float Progress(TitleElement element) const {
        const int index = static_cast<int>(element);
        if (index < 0 || index >= 5) return 0;
        const float t = std::clamp((elapsed_ - boundaries_[index]) /
                                   (boundaries_[index + 1] - boundaries_[index]),0.0F,1.0F);
        return 1 - (1-t)*(1-t)*(1-t);
    }
    /// @brief 菜单行进场进度；index 为局部行号，按行错开，但所有行在 2.6 秒同时完成。
    [[nodiscard]] float MenuProgress(int index) const {
        const float start = boundaries_[4] + std::clamp(index,0,15) * 0.012F;
        const float t = std::clamp((elapsed_ - start) / (boundaries_.back() - start),0.0F,1.0F);
        return 1 - (1-t)*(1-t)*(1-t);
    }
    /// @brief 是否已完成全部入场；输入处理必须在 true 后才接受首页选择、确认或点击。
    [[nodiscard]] bool Finished() const { return elapsed_ >= boundaries_.back(); }
private:
    // 四层与菜单严格顺序且不重叠；2.15～2.6 秒最后显示菜单，之后才解锁操作。
    static constexpr std::array<float,6> boundaries_{0,0.55F,1.0F,1.55F,2.15F,2.6F};
    float elapsed_ = 0;
};
} // namespace koumakyou::ui
