// 本文件提供同文件系统内的文件发布接口，隔离Windows头文件与raylib的同名声明。
#pragma once
#include <filesystem>

namespace koumakyou::platform {
/// @brief 将已完整写入并关闭的source发布到target，两者须在同一文件系统；不创建目录。
/// overwrite=false时不覆盖并发创建的目标；true时原子替换，失败保留原目标并抛异常。
/// 成功后source通常移除，硬链接发布后的清理失败可能保留source，target仍是完整文件。
void PublishFile(const std::filesystem::path& source,const std::filesystem::path& target,bool overwrite);
} // namespace koumakyou::platform
