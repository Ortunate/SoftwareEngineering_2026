// 本文件解析正式素材根目录与独立审阅仓库根目录，不访问游戏状态。
#pragma once
#include <filesystem>
namespace koumakyou::projectiles {
inline std::filesystem::path ProjectileAssetDirectory(const std::filesystem::path& root){if(std::filesystem::exists(root/"projectiles"))return root/"projectiles";if(std::filesystem::exists(root.parent_path()/"projectiles"))return root.parent_path()/"projectiles";return root/"assets/projectiles";}
inline std::filesystem::path ProjectileShaderDirectory(const std::filesystem::path& root){return std::filesystem::exists(root/"shaders/projectiles")?root/"shaders/projectiles":root/"assets/runtime/shaders/projectiles";}
}
