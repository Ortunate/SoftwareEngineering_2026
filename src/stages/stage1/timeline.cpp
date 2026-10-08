// 本文件负责第一面的显式连续出怪、道中 Boss、对话与关底 Boss 时间轴。

#include "stages/stage1/data.hpp"
#include "stages/stage1/enemy_params.hpp"

#include <array>

namespace redmist::stage1 {
namespace {
constexpr StageEventId kMidbossEventId = 1;
constexpr StageEventId kDialogueEventId = 2;
constexpr StageEventId kFinalBossEventId = 3;
using FairyMove = FairyParams::Movement;

// 每条出怪事件只描述同配置的连续实例；普通妖精左右横穿用两条同时启动的事件表达。
// 保留原波次起始间隔和总人数；暗色妖精各出生点均沿 Z 轴往返，出怪默认间隔为 0.5 秒。
// 道中战及对话后的事件仍等待其真实结束，不能改为从遭遇开始计时。
constexpr std::array<StageEvent, 29> kEvents{{
    {.delay = 0.2F, .action = ShowMessage{"Stage 1  Mystic Flier  Normal"}},
    {.delay = 0.8F, .action = SpawnEnemies{
        FairyParams{{0.0F, 1.2F, 8.8F}, FairyMove::Straight}, 8}},
    {.delay = 3.5F, .action = SpawnEnemies{
        FairyParams{{-9.0F, -4.5F, 7.8F}, FairyMove::CrossFromLeft}, 4}},
    {.action = SpawnEnemies{
        FairyParams{{9.0F, 3.15F, 7.8F}, FairyMove::CrossFromRight}, 4}},
    {.delay = 3.5F, .action = SpawnEnemies{
        DarkFairyParams{{0.0F, 2.0F, 8.8F}}, 6}},
    {.delay = 3.5F, .action = SpawnEnemies{
        DarkFairyParams{{-9.0F, -4.5F, 7.8F}}, 3}},
    {.action = SpawnEnemies{
        DarkFairyParams{{9.0F, 3.15F, 7.8F}}, 3}},
    {.delay = 3.5F, .action = SpawnEnemies{
        FairyParams{{0.0F, -0.5F, 8.8F}, FairyMove::Helix}, 8}},
    {.delay = 3.5F, .action = SpawnEnemies{
        FairyParams{{-9.0F, -4.5F, 7.8F}, FairyMove::CrossFromLeft}, 4}},
    {.action = SpawnEnemies{
        FairyParams{{9.0F, 3.15F, 7.8F}, FairyMove::CrossFromRight}, 4}},
    {.delay = 3.5F, .action = SpawnEnemies{
        FairyParams{{0.0F, 1.5F, 8.8F}, FairyMove::Straight}, 10}},
    {.delay = 4.0F, .action = SpawnEnemies{
        FairyParams{{-9.0F, -4.5F, 7.8F}, FairyMove::CrossFromLeft}, 5}},
    {.action = SpawnEnemies{
        FairyParams{{9.0F, 3.15F, 7.8F}, FairyMove::CrossFromRight}, 5}},
    {.delay = 4.0F, .action = StartBoss{}, .id = kMidbossEventId},
    {.delay = 3.0F, .action = SpawnEnemies{
        DarkFairyParams{{0.0F, 1.2F, 8.7F}}, 6},
     .afterEndOf = kMidbossEventId},
    {.delay = 4.0F, .action = SpawnEnemies{
        DarkFairyParams{{-9.0F, -4.5F, 7.8F}}, 4}},
    {.action = SpawnEnemies{
        DarkFairyParams{{9.0F, 3.15F, 7.8F}}, 4}},
    {.delay = 4.0F, .action = SpawnEnemies{
        DarkFairyParams{{0.0F, -0.8F, 8.8F}}, 7}},
    {.delay = 4.0F, .action = SpawnEnemies{
        DarkFairyParams{{-9.0F, -4.5F, 7.8F}}, 4}},
    {.action = SpawnEnemies{
        DarkFairyParams{{9.0F, 3.15F, 7.8F}}, 4}},
    {.delay = 5.0F, .action = SpawnEnemies{
        FairyParams{{0.0F, 1.2F, 8.8F}, FairyMove::Helix}, 10}},
    {.delay = 4.0F, .action = SpawnEnemies{
        FairyParams{{-9.0F, -4.5F, 7.8F}, FairyMove::CrossFromLeft}, 5}},
    {.action = SpawnEnemies{
        FairyParams{{9.0F, 3.15F, 7.8F}, FairyMove::CrossFromRight}, 5}},
    {.delay = 4.0F, .action = SpawnEnemies{
        FairyParams{{0.0F, -1.0F, 8.8F}, FairyMove::Straight}, 10}},
    {.delay = 4.0F, .action = SpawnEnemies{
        FairyParams{{-9.0F, -4.5F, 7.8F}, FairyMove::CrossFromLeft}, 5}},
    {.action = SpawnEnemies{
        FairyParams{{9.0F, 3.15F, 7.8F}, FairyMove::CrossFromRight}, 5}},
    {.delay = 4.0F, .action = SpawnEnemies{
        FairyParams{{0.0F, 1.2F, 8.8F}, FairyMove::Helix}, 8}},
    {.delay = 4.0F, .action = StartDialogue{}, .id = kDialogueEventId},
    {.action = StartBoss{BossEncounter::Final},
     .id = kFinalBossEventId, .afterEndOf = kDialogueEventId},
}};

}  // namespace

std::span<const StageEvent> Stage1Timeline() { return kEvents; }
}  // namespace redmist::stage1
