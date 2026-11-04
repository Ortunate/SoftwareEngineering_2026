# 网站维护

在本目录使用 Node.js 22.12 或更高版本与 npm：

```sh
npm ci
npm run dev
npm test
npm run build
npm run check
npm run test:browser
npm run preview
```

`site.config.mjs` 管理文档分类、网站地址和真实下载条目。新增 docs 文档必须加入分类清单，生成器对遗漏报错。开发文档修改原始 `docs/` 文件；`reference/`、`_generated/`、`public/media/` 是可重建输入，不应手工修改或提交。课程占位页首次生成后作为普通 Markdown 维护。

导航、首页和浏览器标签页复用项目应用图标。`prepare:content` 从 `assets/runtime/common/app_icon.png` 与 `assets/source/app_icon/koumakyou.ico` 复制到 `public/media/`，更新图标后重新构建即可同步。

`docs/course/` 保存从开发分支同步的课程材料原文，分类清单使用 `course/文件名`。同步时一并更新 `courseSourceRevision` 为实际来源提交；该标识仅说明材料来源，不表示网站分支包含同版本游戏代码。不要将新设计材料自动视为缺失会议、交互原型截图或用户反馈的证明。

Windows 浏览测试使用本机 Edge；其他平台先执行 `npx playwright install chromium`。测试会自动启动静态预览。重新构建后须重启已有预览服务，以免保留旧资源索引。Vite 覆盖版本用于修复 VitePress 1.x 所带旧构建依赖，升级 VitePress 时需复核该覆盖配置。

## 队友审阅

开发进度由 `milestones.mjs` 人工归纳关键事件。新增节点需填写阶段成果及少量代表提交，生成器核对 SHA 并提取日期，不将全部提交直接发布到页面。

```sh
cd website
npm ci
npm run dev
```

打开终端输出的本地地址即可查看效果。工作流验证通过后，也可在 Actions 中下载 `website-review`，其中包含静态产物和桌面／手机截图。

## 后续部署

GitHub Actions 在 `feat/wiki-site` 推送时构建验证并发布；`dev/koumakyou` 推送与 PR 仅验证。部署直接使用网站分支，无需合入其他分支。在 Settings → Pages 中选择 GitHub Actions；部署环境如限制分支，需要允许 `feat/wiki-site`。网站不自动更改仓库可见性。

部署后用匿名窗口检查首页、深层链接、搜索和图片，将真实上线日期补入课程材料页，按课程要求保留至少一个月。下载包经验证后填入配置，条目包含 `platform`、`version`、`name`、`url`。
