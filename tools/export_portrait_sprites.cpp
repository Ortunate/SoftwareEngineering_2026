// 本文件只读解码原包 DDS/ANM，将立绘精灵按 ANM 裁图导出 PNG；不需要图形上下文，不改写输入。
#include "resources/animation.hpp"
#include "resources/dds_image.hpp"
#include "raylib.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
/// @brief 参数为 DDS、ANM、输出目录与文件名前缀；逐精灵输出 prefix_ID.png，失败返回非零。
/// 使用 DDS/ANM 尺寸比例裁图，不将高清纹理分辨率误作逻辑坐标；输出目录不得覆盖输入。
int main(int argc,char** argv) {
    if(argc!=5) { std::cerr<<"Usage: export_portrait_sprites DDS ANM OUTPUT_DIRECTORY PREFIX\n";return 2; }
    Image image{};
    try {
        const auto archive=koumakyou::anm::LoadArchive(argv[2]);
        image=koumakyou::LoadOriginalDds(argv[1]);
        if(archive.width<=0||archive.height<=0||!IsImageValid(image))throw std::runtime_error("Invalid portrait dimensions");
        const std::filesystem::path directory=argv[3];std::filesystem::create_directories(directory);
        for(const auto& sprite:archive.sprites) {
            const float sx=static_cast<float>(image.width)/archive.width,sy=static_cast<float>(image.height)/archive.height;
            const Rectangle area{sprite.source.x*sx,sprite.source.y*sy,sprite.source.width*sx,sprite.source.height*sy};
            if(area.x<0||area.y<0||area.width<=0||area.height<=0||area.x+area.width>image.width||area.y+area.height>image.height)
                throw std::runtime_error("Portrait sprite outside DDS");
            const auto output=directory/(std::string(argv[4])+"_"+std::to_string(sprite.id)+".png");
            if(std::filesystem::weakly_canonical(output)==std::filesystem::weakly_canonical(argv[1])||
               std::filesystem::weakly_canonical(output)==std::filesystem::weakly_canonical(argv[2]))
                throw std::runtime_error("Output aliases input");
            Image part=ImageFromImage(image,area);const bool written=ExportImage(part,output.string().c_str());UnloadImage(part);
            if(!written)throw std::runtime_error("Cannot write portrait PNG");
        }
        UnloadImage(image);return 0;
    } catch(const std::exception& error) {
        if(IsImageValid(image))UnloadImage(image);std::cerr<<error.what()<<'\n';return 1;
    }
}
