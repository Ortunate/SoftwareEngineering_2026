// 本文件负责运行按领域拆分的核心测试。

void RunSettingsTests();
void RunGameplayTests();
void RunStageTests();

int main() {
    RunSettingsTests();
    RunGameplayTests();
    RunStageTests();
    return 0;
}
