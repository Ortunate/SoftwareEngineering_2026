# th06nc 的 macOS 包装

`tools/package_th06nc_app.py` 为已安装的游戏生成 `.app`，用于通过 macOS computer use
连接参考游戏。包装依赖本机 CrossOver、原容器和游戏安装，不是可分发的独立游戏。

## 原因与实现

CrossOver 的普通菜单快捷方式通过 Steam 启动游戏，实际游戏进程可以没有 macOS
bundle ID，本机 computer use 不能按其 PID 或窗口编号直接绑定。

此包装在自身 `Contents/MacOS` 保存同版本 Wine loader，通过 CrossOver 的
`--ux-app` 初始化原容器环境并执行它，不启用会在临时目录重命名进程的 alternate
loader。启动脚本使用 `exec`，并切换到游戏安装目录；从 Steam 安装清单读取 App ID，
通过 `SteamAppId` 和 `SteamGameId` 提供给游戏。缺少这些变量时，本次实机检查中
游戏立即返回 255，不能据此认定为 DirectSound 初始化问题。

包装的标识为 `local.heny.th06nc.reference`。必须通过 `.app` 正式启动后验证，
直接在终端执行内部脚本的进程身份可能不同。包装不修改游戏文件或容器配置；
游戏运行本身仍可能正常写入日志、设置或存档。

## 生成与使用

在 macOS 项目根目录运行，将 `--game` 替换为已安装游戏的绝对路径：

```sh
python3 tools/package_th06nc_app.py \
  --output output/th06nc-reference.app \
  --game "/绝对路径/th06nc.exe"
```

默认使用 `/Applications/CrossOver.app` 与 `Steam` 容器，可以分别用
`--crossover`、`--bottle` 指定其他值。已有目标会报错，不会覆盖。

保持原容器里的 Steam 已运行，关闭已有游戏实例，再打开生成的 `.app`。
computer use 应按这个包的绝对路径连接，例如：

```javascript
var reference = await cua.getApp('/绝对路径/th06nc-reference.app');
```

CrossOver 升级后需要重新生成到新目录并验证，因为包内 loader 与借用的 ntdll
必须保持同版本。生成目录已被 Git 忽略；只提交生成脚本与说明，不提交运行时二进制。

## 本次验证

2026-10-09，在本机已安装的 CrossOver 环境完成以下检查：

- `Info.plist` 通过 `plutil -lint`，启动脚本通过 `sh -n`。
- computer use 通过包的绝对路径成功取得真实游戏窗口的 AX 状态和标题截图。
- `reference.pressKey('z', {durationMs:150})` 后，截图显示从标题进入主菜单，
  确认输入已到达游戏。没有用原生截图或键盘接口代替这项验证。

其他机器或 CrossOver 版本需要重复这项实际窗口与输入检查。
