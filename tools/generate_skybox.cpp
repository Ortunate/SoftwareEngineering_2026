// 本文件负责生成六面连续的红雾天空盒贴图。

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace {

constexpr int kFaceSize = 1024;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Rgb {
    float r;
    float g;
    float b;
};

float Clamp01(float value) {
    return std::clamp(value, 0.0F, 1.0F);
}

float Smoothstep(float low, float high, float value) {
    const float t = Clamp01((value - low) / (high - low));
    return t * t * (3.0F - 2.0F * t);
}

Rgb Mix(Rgb first, Rgb second, float amount) {
    const float t = Clamp01(amount);
    return {first.r + (second.r - first.r) * t,
            first.g + (second.g - first.g) * t,
            first.b + (second.b - first.b) * t};
}

Rgb Add(Rgb first, Rgb second, float amount) {
    return {first.r + second.r * amount,
            first.g + second.g * amount,
            first.b + second.b * amount};
}

std::uint32_t Hash(int x, int y, int z) {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b1U ^
                          static_cast<std::uint32_t>(y) * 0x85ebca77U ^
                          static_cast<std::uint32_t>(z) * 0xc2b2ae3dU;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

float Random01(int x, int y, int z = 0) {
    return static_cast<float>(Hash(x, y, z) & 0xffffU) / 65535.0F;
}

float Noise(Vec3 p) {
    const int ix = static_cast<int>(std::floor(p.x));
    const int iy = static_cast<int>(std::floor(p.y));
    const int iz = static_cast<int>(std::floor(p.z));
    const float tx = Smoothstep(0.0F, 1.0F, p.x - ix);
    const float ty = Smoothstep(0.0F, 1.0F, p.y - iy);
    const float tz = Smoothstep(0.0F, 1.0F, p.z - iz);

    const float a = Random01(ix, iy, iz);
    const float b = Random01(ix + 1, iy, iz);
    const float c = Random01(ix, iy + 1, iz);
    const float d = Random01(ix + 1, iy + 1, iz);
    const float e = Random01(ix, iy, iz + 1);
    const float f = Random01(ix + 1, iy, iz + 1);
    const float g = Random01(ix, iy + 1, iz + 1);
    const float h = Random01(ix + 1, iy + 1, iz + 1);
    const float lower = (a + (b - a) * tx) * (1.0F - ty) +
                        (c + (d - c) * tx) * ty;
    const float upper = (e + (f - e) * tx) * (1.0F - ty) +
                        (g + (h - g) * tx) * ty;
    return lower + (upper - lower) * tz;
}

float Fbm(Vec3 p) {
    float value = 0.0F;
    float weight = 0.55F;
    for (int octave = 0; octave < 4; ++octave) {
        value += Noise(p) * weight;
        p = {p.x * 2.03F + 11.7F,
             p.y * 2.03F - 5.3F,
             p.z * 2.03F + 2.9F};
        weight *= 0.5F;
    }
    return value / 1.03125F;
}

Vec3 Normalize(Vec3 p) {
    const float scale = 1.0F / std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    return {p.x * scale, p.y * scale, p.z * scale};
}

Vec3 FaceDirection(int face, float x, float y) {
    // OpenGL cubemap order: +X, -X, +Y, -Y, +Z, -Z.
    // x and y increase to the right and down in the output image.
    switch (face) {
        case 0: return Normalize({ 1.0F, -y, -x});
        case 1: return Normalize({-1.0F, -y,  x});
        case 2: return Normalize({ x,  1.0F,  y});
        case 3: return Normalize({ x, -1.0F, -y});
        case 4: return Normalize({ x, -y,  1.0F});
        default: return Normalize({-x, -y, -1.0F});
    }
}

float AngularDistance(float a, float b) {
    return std::abs(std::atan2(std::sin(a - b), std::cos(a - b)));
}

float Ridge(float longitude, int layer) {
    const float offset = static_cast<float>(layer) * 2.47F;
    float height = std::sin(longitude * 4.0F + offset) * 0.014F +
                   std::sin(longitude * 11.0F - offset) * 0.011F +
                   std::sin(longitude * 29.0F + offset) * 0.006F;
    for (int index = 0; index < 15; ++index) {
        const float center = -std::numbers::pi_v<float> +
                             (2.0F * std::numbers::pi_v<float> *
                              (static_cast<float>(index) +
                               Random01(index, layer, 19) * 0.5F)) / 15.0F;
        const float width = 0.08F + Random01(index, layer, 23) * 0.14F;
        const float peak = 1.0F - AngularDistance(longitude, center) / width;
        height += std::pow(Clamp01(peak), 1.25F) *
                  (0.025F + Random01(index, layer, 29) * 0.075F);
    }
    return height;
}

float TreeLine(float longitude) {
    constexpr float treeCount = 330.0F;
    const float index = (longitude + std::numbers::pi_v<float>) *
                        treeCount / (2.0F * std::numbers::pi_v<float>);
    const int tree = static_cast<int>(std::floor(index));
    float crown = -1.0F;
    for (int i = tree - 1; i <= tree + 1; ++i) {
        const int wrapped = (i + static_cast<int>(treeCount)) %
                            static_cast<int>(treeCount);
        const float center = static_cast<float>(i) +
                             0.25F + 0.5F * Random01(wrapped, 6);
        const float halfWidth = 0.35F + 0.15F * Random01(wrapped, 7);
        const float triangle = Clamp01(1.0F - std::abs(index - center) / halfWidth);
        crown = std::max(crown, triangle *
                         (0.025F + 0.04F * Random01(wrapped, 8)));
    }
    return crown;
}

Color Sample(Vec3 direction) {
    const float elevation = std::asin(std::clamp(direction.y, -1.0F, 1.0F));
    const float longitude = std::atan2(direction.x, direction.z);
    const Rgb nearHorizon{105.0F, 27.0F, 58.0F};
    const Rgb upperSky{27.0F, 10.0F, 35.0F};
    Rgb color = Mix(nearHorizon, upperSky,
                    Smoothstep(-0.12F, 1.15F, elevation));

    const Vec3 moon = Normalize({-0.19F, 0.33F, 1.0F});
    const float moonAngle = std::acos(std::clamp(direction.x * moon.x +
        direction.y * moon.y + direction.z * moon.z, -1.0F, 1.0F));
    const float halo = 1.0F - Smoothstep(0.08F, 0.45F, moonAngle);
    color = Add(color, {103.0F, 27.0F, 36.0F}, halo * halo * 0.55F);

    const float moonShape = 1.0F - Smoothstep(0.072F, 0.076F, moonAngle);
    if (moonShape > 0.0F) {
        const float crater = Fbm({direction.x * 75.0F,
                                 direction.y * 75.0F,
                                 direction.z * 75.0F});
        color = Mix(color, {189.0F + crater * 37.0F,
                            111.0F + crater * 41.0F,
                            123.0F + crater * 31.0F}, moonShape);
    }

    if (elevation > -0.12F) {
        const Vec3 cloudPoint{direction.x * 3.5F,
                              direction.y * 11.0F,
                              direction.z * 3.5F};
        const float body = Fbm(cloudPoint);
        const float wisps = Fbm({direction.x * 9.0F + 6.0F,
                                 direction.y * 26.0F,
                                 direction.z * 9.0F});
        const float horizonBand = 1.0F - Smoothstep(0.25F, 1.0F, elevation);
        const float cloud = Smoothstep(0.46F, 0.64F,
            body * 0.72F + wisps * 0.28F + 0.06F * horizonBand);
        const float lit = Clamp01(halo * 0.9F +
                         (wisps - 0.34F) * 0.75F + 0.1F);
        color = Mix(color, Mix({34.0F, 7.0F, 29.0F},
                               {124.0F, 23.0F, 49.0F}, lit),
                    cloud * (0.48F + 0.26F * horizonBand));
    }

    const float farLine = -0.055F + Ridge(longitude, 0);
    if (elevation < farLine) {
        const float haze = 0.5F + 0.5F * Fbm({direction.x * 12.0F,
                                              direction.y * 12.0F,
                                              direction.z * 12.0F});
        color = Mix(color, {75.0F + haze * 18.0F,
                            24.0F + haze * 5.0F,
                            49.0F + haze * 9.0F},
                    Smoothstep(farLine, farLine - 0.014F, elevation));
    }
    const float nearLine = -0.17F + Ridge(longitude, 1);
    if (elevation < nearLine) {
        color = Mix(color, {28.0F, 11.0F, 29.0F},
                    Smoothstep(nearLine, nearLine - 0.025F, elevation));
    }
    const float forestLine = -0.18F + TreeLine(longitude);
    if (elevation < forestLine) {
        color = Mix(color, {17.0F, 7.0F, 21.0F},
                    Smoothstep(forestLine, forestLine - 0.01F, elevation));
    }

    const float fogNoise = Fbm({direction.x * 14.0F,
                                direction.y * 7.0F,
                                direction.z * 14.0F});
    const float fog = std::exp(-std::pow((elevation + 0.32F) * 6.0F, 2.0F)) *
                      (0.35F + fogNoise * 0.3F);
    color = Mix(color, {82.0F, 28.0F, 60.0F}, fog);
    color = Mix(color, {24.0F, 9.0F, 28.0F},
                Smoothstep(-0.55F, -1.1F, elevation) * 0.85F);

    return {static_cast<unsigned char>(std::clamp(color.r, 0.0F, 255.0F)),
            static_cast<unsigned char>(std::clamp(color.g, 0.0F, 255.0F)),
            static_cast<unsigned char>(std::clamp(color.b, 0.0F, 255.0F)), 255};
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    Image strip = GenImageColor(kFaceSize * 6, kFaceSize, BLACK);
    if (strip.data == nullptr) return 1;
    auto* pixels = static_cast<Color*>(strip.data);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < kFaceSize; ++y) {
            for (int x = 0; x < kFaceSize; ++x) {
                const float u = 2.0F * (static_cast<float>(x) + 0.5F) /
                                kFaceSize - 1.0F;
                const float v = 2.0F * (static_cast<float>(y) + 0.5F) /
                                kFaceSize - 1.0F;
                pixels[y * kFaceSize * 6 + face * kFaceSize + x] =
                    Sample(FaceDirection(face, u, v));
            }
        }
    }
    const bool success = ExportImage(strip, argv[1]);
    UnloadImage(strip);
    return success ? 0 : 1;
}
