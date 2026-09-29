// 本文件负责裁剪并缩放弹幕原图，生成运行时使用的贴图。

#include "raylib.h"

#include <array>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const std::filesystem::path sourceRoot = argv[1];
    const std::filesystem::path outputRoot = argv[2];
    std::filesystem::create_directories(outputRoot);
    struct SpriteSource {
        const char* input;
        const char* output;
        Rectangle crop;
    };
    constexpr std::array<SpriteSource, 2> sources{{
        {"bullet_pearl.png", "bullet_pearl_runtime.png",
         {227.0F, 227.0F, 800.0F, 800.0F}},
        {"bullet_red.png", "bullet_red_runtime.png",
         {77.0F, 77.0F, 1100.0F, 1100.0F}},
    }};

    for (const SpriteSource& source : sources) {
        Image image = LoadImage((sourceRoot / source.input).string().c_str());
        if (!IsImageValid(image)) return 1;
        ImageCrop(&image, source.crop);
        ImageResize(&image, 512, 512);
        const bool exported = ExportImage(image,
                                          (outputRoot / source.output).string().c_str());
        UnloadImage(image);
        if (!exported) return 1;
    }
    return 0;
}
