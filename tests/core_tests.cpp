// 本文件负责运行按领域拆分的核心测试。
#include "render/ui_motion.hpp"
#include <cassert>
#include <cmath>

namespace {
/// @brief 检查同一真实时长在 30/240 FPS 下的选择效果一致，以及快速切换不会保留上一页行号。
void TestMenuMotion() {
    koumakyou::ui::MenuMotion slow, fast;
    slow.Advance(0,0,0); fast.Advance(0,0,0);
    for (int frame=0;frame<6;++frame) slow.Advance(0,8,1.0F/30);
    for (int frame=0;frame<48;++frame) fast.Advance(0,8,1.0F/240);
    assert(std::abs(slow.Weight(8)-fast.Weight(8))<0.00001F);
    assert(std::abs(slow.Reveal()-fast.Reveal())<0.00001F);
    assert(slow.Weight(8)>0.95F && slow.Weight(0)<0.05F);
    slow.Advance(0,2,1.0F/240); slow.Advance(0,9,1.0F/240);
    slow.Advance(1,1,0);
    assert(slow.Weight(8)==0 && slow.Weight(9)==0 && slow.Weight(1)==1);
    assert(slow.Reveal()==0);
    slow.Advance(1,1,0.5F);
    assert(slow.Reveal(0.12F)==1);
    slow.Confirm(); slow.Advance(1,1,0.2F);
    assert(slow.Flash()==0);
}
}

/// @brief 运行设置读写和键位校验用例，断言失败时终止进程。
void RunSettingsTests();
/// @brief 运行战斗、弹幕和 Boss 行为用例，断言失败时终止进程。
void RunGameplayTests();
/// @brief 运行道具分布、生成与拾取结算用例，断言失败时终止进程。
void RunDropTests();
/// @brief 运行玩家封装后的移动、射击与受击用例，断言失败时终止进程。
void RunPlayerTests();
/// @brief 运行时间轴及关卡内容用例，断言失败时终止进程。
void RunStageTests();
/// @brief 运行入退场曲线、快照与回收用例，断言失败时终止进程。
void RunActorEffectTests();

int main() {
    TestMenuMotion();
    RunSettingsTests();
    RunGameplayTests();
    RunDropTests();
    RunPlayerTests();
    RunStageTests();
    RunActorEffectTests();
    return 0;
}
