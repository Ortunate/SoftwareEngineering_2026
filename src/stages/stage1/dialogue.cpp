// 本文件提供第一面灵梦与露米娅的中文改写对话及说话角色；保持双立绘交替。

#include "stages/stage1/data.hpp"

#include <array>

namespace redmist::stage1 {
namespace {
constexpr std::array<DialogueLine, 6> kDialogue{{
    {"灵梦", "太阳下山以后，连路都看不清了。", false},
    {"灵梦", "红雾里似乎还有别的气息。", false},
    {"露米娅", "又见面了。你能看见躲在黑暗里的我？", true},
    {"灵梦", "是你让这片天空变得这么暗的吗？", false},
    {"露米娅", "黑暗是我的地盘，你想从这里过去？", true},
    {"灵梦", "那就先把挡路的你解决掉！", false},
}};
}  // namespace

std::span<const DialogueLine> Stage1Dialogue() { return kDialogue; }
}  // namespace redmist::stage1
