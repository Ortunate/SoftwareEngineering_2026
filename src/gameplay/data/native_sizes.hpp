// 本文件保存新典尺寸到三维战场的统一比例及已核对的判定常量；不依赖本机原作或高清贴图分辨率。
#pragma once

namespace koumakyou::native_sizes {
/// @brief 原作 448 逻辑像素纵深对应 20 世界单位；战斗显示、判定与尺寸偏移共用此比例。
inline constexpr float kWorldPerPixel=20.0F/448;
/// @brief EXE RVA 0x69a24 的自机圆形半径 1.25；擦弹在命中半径外增加 20 逻辑像素。
inline constexpr float kPlayerHitRadius=0.0558035714F, kPlayerGrazeRadius=0.948660714F;
/// @brief 原生默认敌机全宽 12 的内接球，以及 ECL 全宽 28/40/45/48/56 的内接球，单位为世界半径。
/// 三维球形适配取原 X/Y 全宽的较小值除二；源 Z=32 不用作球形直径，避免压小 Boss 判定。
inline constexpr float kDefaultEnemyRadius=0.267857143F, kEnemyRadius=0.625F;
inline constexpr float kBoss40Radius=0.892857143F, kBoss45Radius=1.00446429F;
inline constexpr float kBoss48Radius=1.07142857F, kBoss56Radius=1.25F;
/// @brief 原生本体接触使用受弹判定全宽的 1/1.5；不影响追踪、玩家弹或 Bomb 的受弹范围。
inline constexpr float kBodyContactScale=2.0F/3;
/// @brief 道具全宽 16 与原生自机拾取半宽 12 合成接触距离 20，采用球形世界适配。
inline constexpr float kItemCollectRadius=0.892857143F;
/// @brief 双副机 ANM 基础直径为 16 逻辑像素，独立于裁图的 36 图集像素。
inline constexpr float kOptionHeight=0.714285714F;
}
