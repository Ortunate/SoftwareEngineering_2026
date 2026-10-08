// 本文件负责运行按领域拆分的核心测试。

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
    RunSettingsTests();
    RunGameplayTests();
    RunDropTests();
    RunPlayerTests();
    RunStageTests();
    RunActorEffectTests();
    return 0;
}
