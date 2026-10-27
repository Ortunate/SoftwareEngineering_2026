// 本文件负责注册并构造可供游戏总控选择的关卡内容。

#include "stage/catalog.hpp"

#include "stages/stage1/content.hpp"
#include "stages/stage2/content.hpp"
#include "stages/stage3/content.hpp"
#include "stages/stage4/content.hpp"
#include "stages/stage5/content.hpp"
#include "stages/stage6/content.hpp"

#include <stdexcept>

namespace koumakyou {

std::unique_ptr<StageContent> CreateStageContent(StageId id) {
    switch (id) {
        case StageId::Stage1: return std::make_unique<stage1::Stage1Content>();
        case StageId::Stage2: return std::make_unique<stage2::Stage2Content>();
        case StageId::Stage3: return std::make_unique<stage3::Stage3Content>();
        case StageId::Stage4: return std::make_unique<stage4::Stage4Content>();
        case StageId::Stage5: return std::make_unique<stage5::Stage5Content>();
        case StageId::Stage6: return std::make_unique<stage6::Stage6Content>();
    }
    throw std::invalid_argument("Unknown StageId");
}

}  // namespace koumakyou
