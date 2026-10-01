// 本文件负责第一面的对话文本与说话角色。

#include "stages/stage1/data.hpp"

#include <array>

namespace redmist::stage1 {
namespace {
constexpr std::array<DialogueLine, 6> kDialogue{{
    {"REIMU", "The mist is getting thicker...", false},
    {"REIMU", "Someone is hiding in the dark.", false},
    {"RUMIA", "You can see me from there?", true},
    {"REIMU", "Are you behind this strange night?", false},
    {"RUMIA", "Maybe. I like the darkness.", true},
    {"REIMU", "Then I'll clear a path through it!", false},
}};
}  // namespace

std::span<const DialogueLine> Stage1Dialogue() { return kDialogue; }
}  // namespace redmist::stage1
