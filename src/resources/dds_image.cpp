// 本文件从原始 DDS 解码 BGRA 与 BC 压缩块，保留 Alpha 并检查所有读取边界。
#include "resources/dds_image.hpp"
#define BCDEC_IMPLEMENTATION
#include "../../third_party/bcdec/bcdec.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace koumakyou {
namespace {
std::uint32_t Read(const std::vector<unsigned char>& data,std::size_t offset) {
    if(offset>data.size() || data.size()-offset<4)throw std::runtime_error("Truncated DDS");
    return data[offset]|std::uint32_t(data[offset+1])<<8|std::uint32_t(data[offset+2])<<16|std::uint32_t(data[offset+3])<<24;
}
Color Rgb565(unsigned value) {
    const unsigned r=(value>>11)&31,g=(value>>5)&63,b=value&31;
    return {static_cast<unsigned char>((r<<3)|(r>>2)),static_cast<unsigned char>((g<<2)|(g>>4)),
            static_cast<unsigned char>((b<<3)|(b>>2)),255};
}
Color Mix(Color a,Color b,int first,int second,int divisor) {
    return {static_cast<unsigned char>((a.r*first+b.r*second)/divisor),
            static_cast<unsigned char>((a.g*first+b.g*second)/divisor),
            static_cast<unsigned char>((a.b*first+b.b*second)/divisor),255};
}
}
Image LoadOriginalDds(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("Missing DDS: "+path.string());
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(stream),{}};
    if(bytes.size()<128 || Read(bytes,0)!=0x20534444 || Read(bytes,4)!=124)
        throw std::runtime_error("Invalid DDS header");
    const unsigned height=Read(bytes,12),width=Read(bytes,16),flags=Read(bytes,80),fourcc=Read(bytes,84);
    if(!width || !height || width>8192 || height>8192)throw std::runtime_error("Invalid DDS dimensions");
    const bool dx10=fourcc==0x30315844;
    const unsigned dxgi=dx10 ? Read(bytes,128) : 0;
    if(dx10 && (bytes.size()<148 || Read(bytes,132)!=3 || Read(bytes,140)!=1))
        throw std::runtime_error("Unsupported DDS array or texture dimension");
    const std::size_t dataStart=dx10 ? 148 : 128;
    const bool raw=((flags&64) && Read(bytes,88)==32) || dxgi==28 || dxgi==87;
    const bool rgba=dxgi==28;
    const int bc=fourcc==0x31545844 || dxgi==71 || dxgi==72 ? 1 :
                 fourcc==0x33545844 || dxgi==74 || dxgi==75 ? 2 :
                 fourcc==0x35545844 || dxgi==77 || dxgi==78 ? 3 : dxgi==98 || dxgi==99 ? 7 : 0;
    if(!raw && !bc)throw std::runtime_error("Unsupported DDS encoding");
    if(raw && !dx10 && (Read(bytes,92)!=0xff0000 || Read(bytes,96)!=0xff00 || Read(bytes,100)!=0xff))
        throw std::runtime_error("Unsupported DDS channel masks");
    const std::size_t blocksX=(width+3)/4,blocksY=(height+3)/4;
    const std::size_t needed=raw ? std::size_t(width)*height*4 : blocksX*blocksY*(bc==1 ? 8 : 16);
    if(bytes.size()<dataStart || bytes.size()-dataStart<needed)throw std::runtime_error("Truncated DDS pixel data");
    auto* pixels=static_cast<unsigned char*>(MemAlloc(width*height*4));
    if(!pixels)throw std::runtime_error("DDS allocation failed");
    if(raw) {
        for(std::size_t i=0;i<std::size_t(width)*height;++i) {
            pixels[i*4]=bytes[dataStart+i*4+(rgba ? 0 : 2)];pixels[i*4+1]=bytes[dataStart+i*4+1];
            pixels[i*4+2]=bytes[dataStart+i*4+(rgba ? 2 : 0)];pixels[i*4+3]=bytes[dataStart+i*4+3];
        }
    } else {
        for(std::size_t by=0;by<blocksY;++by)for(std::size_t bx=0;bx<blocksX;++bx) {
            const auto block=dataStart+(by*blocksX+bx)*(bc==1 ? 8 : 16);
            if(bc==7) {
                alignas(16) std::array<unsigned char,16> compressed{};
                alignas(16) std::array<unsigned char,64> decoded{};
                std::memcpy(compressed.data(),bytes.data()+block,16);
                bcdec_bc7(compressed.data(),decoded.data(),16);
                for(unsigned i=0;i<16;++i) {
                    const unsigned x=bx*4+i%4,y=by*4+i/4;
                    if(x<width && y<height)std::memcpy(pixels+(std::size_t(y)*width+x)*4,decoded.data()+i*4,4);
                }
                continue;
            }
            const auto colorAt=block+(bc==1 ? 0 : 8);
            const unsigned first=bytes[colorAt]|unsigned(bytes[colorAt+1])<<8;
            const unsigned second=bytes[colorAt+2]|unsigned(bytes[colorAt+3])<<8;
            std::array<Color,4> colors{Rgb565(first),Rgb565(second),{}, {0,0,0,0}};
            if(first>second || bc!=1) {colors[2]=Mix(colors[0],colors[1],2,1,3);colors[3]=Mix(colors[0],colors[1],1,2,3);}
            else colors[2]=Mix(colors[0],colors[1],1,1,2);
            const auto indices=Read(bytes,colorAt+4);
            std::array<unsigned char,8> alpha{};std::uint64_t alphaBits=0;
            if(bc==3) {
                alpha[0]=bytes[block];alpha[1]=bytes[block+1];
                if(alpha[0]>alpha[1])for(int i=2;i<8;++i)alpha[i]=((8-i)*alpha[0]+(i-1)*alpha[1])/7;
                else {for(int i=2;i<6;++i)alpha[i]=((6-i)*alpha[0]+(i-1)*alpha[1])/5;alpha[6]=0;alpha[7]=255;}
                for(int i=0;i<6;++i)alphaBits|=std::uint64_t(bytes[block+2+i])<<(i*8);
            }
            for(unsigned i=0;i<16;++i) {
                const unsigned x=bx*4+i%4,y=by*4+i/4;
                if(x>=width || y>=height)continue; // 非 4 倍数边长的边块只写有效像素。
                Color color=colors[(indices>>(i*2))&3];
                if(bc==2)color.a=((bytes[block+i/2]>>((i%2)*4))&15)*17;
                if(bc==3)color.a=alpha[(alphaBits>>(i*3))&7];
                const auto pixel=(std::size_t(y)*width+x)*4;
                pixels[pixel]=color.r;pixels[pixel+1]=color.g;pixels[pixel+2]=color.b;pixels[pixel+3]=color.a;
            }
        }
    }
    return {pixels,static_cast<int>(width),static_cast<int>(height),1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
} // namespace koumakyou
