#include "game/stage_catalog.hpp"

#include "game/stages/stage1_content.hpp"

#include <stdexcept>

namespace redmist {

std::unique_ptr<StageContent> CreateStageContent(StageId id) {
    switch (id) {
        case StageId::Stage1: return std::make_unique<stage1::Stage1Content>();
    }
    throw std::invalid_argument("Unknown StageId");
}

}  // namespace redmist
