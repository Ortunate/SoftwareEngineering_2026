# 小芽口算

五个给定题目中第 2 项「儿童学习」的候选实现，当前版本 2.0.1（三年成长型练习与提示修订）。面向小学低年级学生与家长，提供口算出题、批改、错题巩固及成长记录。独立存放在 `savior/`，尚不代表小组最终选题。

选择理由：目标用户和每日 30 题的场景明确；出题与判题规则能够独立验证；软件工程各阶段有具体内容；日常调试不依赖校园网。

**在线试用：https://ortunate.github.io/SoftwareEngineering_2026/**

已于 2026-09-19 发布到 GitHub Pages。私有源码仓库保持私有，网页公开访问。指定分支 `兜底选题-儿童口算练习` 更新 `savior/` 时，由 `.github/workflows/savior-pages.yml` 自动测试、构建并发布 `savior/dist/`。线上与本地记录按网址隔离，需要用 JSON 备份迁移。

## 启动

最简单：双击本目录的 `index.html`，不需要安装依赖。浏览器对本地文件存储的支持存在差异，建议使用下面的固定地址启动方式保存长期记录。

在仓库根目录运行：

```powershell
node savior/scripts/serve.cjs
```

然后打开 **http://127.0.0.1:4173**。服务仅监听本机，按 `Ctrl+C` 停止。需要 Node.js 18 或更高版本。运行时不使用 npm 第三方包、CDN、云数据库或在线 AI 接口。

若端口占用，在 PowerShell 中设置 `$env:PORT = '4174'` 后重启，并打开相应地址。不同端口的浏览器记录互相独立。

## 已实现

- 八个成长阶段，10/20/50/100/1000/10000范围，5–50题；支持多位数乘除一位数、两至三步混合、括号和负数拓展。
- 每套题不重复；操作数与中间结果受数域限制；除法限定整除；批改后可查看计算步骤。
- 答题进度、空题提交确认、自动批改、逐题反馈、重复提交保护。
- 可见计时器；切换应用页面或隐藏标签时自动暂停；Tab 下一题、Shift+Tab 上一题、Enter 提交提示。
- 基于准确率与题型参考用时给出本次表现；准确率独立计算，优先保证正确，旧记录与纸笔记录不做速度判断。
- 原错题加同类变式题巩固；原题连续答对两次后标记已巩固，再次答错则重新加入。
- 今日统计、历史练习、题型正确率和长期阶段累计；阶段进度不受200次明细上限影响。旧版记录自动兼容。
- 题单打印与独立答案页；JSON 备份导入导出、格式校验、清空确认。
- 无账号、无广告、无在线数据上传；桌面与移动端响应式布局。

范围说明：年级标签为建议路线，不等同于教材进度；负数为课外选学，多位数及复合题可笔算。实现支持长期分阶段练习，不声称已验证三年效果或覆盖全部数学课程。详见[三年成长设计](docs/requirements/growth-plan.md)。

## 测试与发布产物

在仓库根目录运行：

```powershell
node savior/tests/run.cjs
node savior/scripts/build.cjs
```

或在 `savior/` 中使用 `npm.cmd test`、`npm.cmd start`、`npm.cmd run build`。

构建输出在 `dist/`，包含 HTML、CSS、JavaScript 和文件校验清单，可作为静态网站发布；`dist/` 已忽略，可随时重建。

**自动化测试已覆盖核心算法和本地 HTTP 服务。浏览器交互、移动端布局和实际打印尚需人工验收，不将代码测试视为界面测试通过。**请按[手动验收清单](docs/testing/manual-acceptance.md)操作并记录结果。

2.0 的26项代码测试通过，包含8阶段×20随机种子×50题，以及运算顺序、负数、迁移和长期累计。新增交互请按[2.0手动验收](docs/testing/growth-acceptance.md)检查。浏览器测试按用户要求手动进行，不使用 Computer Use。

## 文件分类

```text
savior/
├── index.html                 应用入口
├── src/                       界面、样式、业务规则
├── scripts/                   本地服务器、静态发布构建
├── tests/                     可重复的自动化测试
├── docs/
│   ├── requirements/          需求规格、NABCD、调研计划
│   ├── design/                架构、数据、算法、交互原型说明
│   ├── project/               选题依据、任务分解、过程记录
│   ├── testing/               测试计划、测试报告、人工验收
│   └── release/               用户手册、发布与演示指南
└── dist/                      自动生成的可发布静态文件
```

## 文档入口

| 文档 | 内容 |
| --- | --- |
| [需求规格说明书](docs/requirements/requirements.md) | 用户、范围、功能、非功能、验收标准 |
| [NABCD 分析](docs/requirements/nabcd.md) | 项目需求与价值分析，含课堂 N 部分 |
| [用户调研与反馈计划](docs/requirements/research.md) | 访谈提纲、验证方法、真实反馈记录模板 |
| [系统设计](docs/design/architecture.md) | 分层、数据模型、关键算法、接口、设计决策 |
| [交互原型说明](docs/design/prototype.md) | 页面、完整交互流程与错误状态 |
| [项目计划](docs/project/plan.md) | 选题依据、36 项任务、角色建议、甘特图 |
| [过程记录](docs/project/process.md) | AI 使用说明、会议模板、课程交付检查 |
| [测试计划](docs/testing/test-plan.md) | 自动化覆盖与需求追踪 |
| [测试报告](docs/testing/test-report.md) | 实际测试结果及未验证事项 |
| [手动验收清单](docs/testing/manual-acceptance.md) | 请用户执行的浏览器与打印测试 |
| [多分辨率验收](docs/testing/responsive-acceptance.md) | Windows Chrome、系统缩放、分屏与跨平台手动检查 |
| [用户手册](docs/release/user-guide.md) | 操作、数据保存、常见问题 |
| [发布与演示](docs/release/release-plan.md) | 静态发布、课程发布要求、8 分钟演示 |
| [课程报告提纲](docs/report-outline.md) | 依课程样本组织各章及证据材料 |

下一阶段需真实组员和用户参与：确定最终选题、家长需求访谈、教师评审、完善人工验收和收集公开试用反馈。当前没有虚构的调研统计、用户评价或组员分工。
