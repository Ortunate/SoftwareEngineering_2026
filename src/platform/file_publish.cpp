// 本文件实现同目录文件的原子发布；Windows使用MoveFileExW，其他平台用rename或无覆盖硬链接。
#include "platform/file_publish.hpp"
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace koumakyou::platform {
void PublishFile(const std::filesystem::path& source,const std::filesystem::path& target,bool overwrite) {
#ifdef _WIN32
    if(!MoveFileExW(source.c_str(),target.c_str(),overwrite?MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH:MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("文件发布失败");
#else
    if(overwrite)std::filesystem::rename(source,target);
    else {
        // 同文件系统的硬链接创建具有不覆盖语义；不能先检查存在性再用会覆盖的rename发布。
        std::filesystem::create_hard_link(source,target);
        std::error_code ignored;std::filesystem::remove(source,ignored);
    }
#endif
}
} // namespace koumakyou::platform
