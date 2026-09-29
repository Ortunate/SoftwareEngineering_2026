// 本文件负责注册并构造可供游戏总控选择的关卡内容。

#include "stage/stage_catalog.hpp"

#include "stages/stage1/stage1_content.hpp"

#include <stdexcept>

namespace redmist {

std::unique_ptr<StageContent> CreateStageContent(StageId id) {
    switch (id) {
        case StageId::Stage1: return std::make_unique<stage1::Stage1Content>();
    }
    throw std::invalid_argument("Unknown StageId");
}

}  // namespace redmist
