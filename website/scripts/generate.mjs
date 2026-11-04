// 从现有文档和 Git 历史生成网站输入；只重建 website 内指定的派生目录，不修改源文档。
import fs from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { execFileSync } from 'node:child_process'
import { site, groups, coursePages, courseSourceRevision } from '../site.config.mjs'
import { resolveLink, parseHistory } from './content.mjs'
import { milestones } from '../milestones.mjs'

const web = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const root = path.dirname(web)
const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 }).trimEnd()
const revision = git('rev-parse', 'HEAD')
const publishedFiles = new Set(git('ls-tree', '-r', '--name-only', revision).split('\n'))
const names = groups.flatMap(([, files]) => files)
const actual = (await fs.readdir(path.join(root, 'docs'))).filter(f => f.endsWith('.md'))
actual.push(...(await fs.readdir(path.join(root, 'docs/course'))).filter(f => f.endsWith('.md')).map(f => `course/${f}`))
const missing = actual.filter(f => !names.includes(f.slice(0, -3)))
if (missing.length) throw new Error(`请将新文档加入 site.config.mjs：${missing.join(', ')}`)
if (new Set(names).size !== names.length) throw new Error('文档清单重复')
// 删除范围固定在网站派生目录内；构建产物不作为可编辑内容存储。
for (const dir of ['reference', '_generated', 'public/media']) {
  const target = path.resolve(web, dir)
  if (!target.startsWith(web + path.sep)) throw new Error('派生目录越界')
  await fs.rm(target, { recursive: true, force: true })
  await fs.mkdir(target, { recursive: true })
}
const pages = new Map(names.map(name => [`docs/${name}.md`, `reference/${name}.html`]))
pages.set('README.md', 'reference/README.html')
const titles = {}
for (const [source, route] of pages) {
  const isCourse = source.startsWith('docs/course/')
  const sourceRevision = isCourse ? courseSourceRevision : revision
  const sourceFiles = isCourse ? new Set(git('ls-tree', '-r', '--name-only', sourceRevision).split('\n')) : publishedFiles
  let body = await fs.readFile(path.join(root, source), 'utf8')
  const title = body.match(/^#\s+(.+)$/m)?.[1] || source
  titles[source] = title
  // 用 Markdown 链接替换器只改链接目标；代码块保持原文且禁止 Vue 模板求值。
  const chunks = body.split(/(^```[^\n]*\n[\s\S]*?^```\s*$)/m)
  body = chunks.map((chunk, i) => i % 2 ? chunk : chunk.replace(/</g, '&lt;').replace(/\{\{/g, '&#123;&#123;').replace(/(!?)\[([^\]]*)\]\(([^)]+)\)/g, (all, image, label, href) => {
    const result = resolveLink(href, source, pages, site, sourceRevision)
    if (!result) return `${label}（本机路径，见源文档）`
    const blobPrefix = `${site.repository}/blob/${sourceRevision}/`
    if (result.startsWith(blobPrefix)) {
      const target = decodeURIComponent(result.slice(blobPrefix.length).split('#')[0])
      if (!sourceFiles.has(target)) return `${label || '附件'}（来源提交未包含该文件）`
    }
    // 非站内图片保留来源链接，避免把源码 HTML 页面误作图片请求。
    if (image && result.includes('/blob/')) return `[${label || '图片来源'}](${result})`
    return `${image}[${label}](${result})`
  })).join('')
  let note = '此页从仓库原文生成；其中的验证记录仅适用于原文注明的版本与环境。'
  if (source.endsWith('SOFTWARE_DESIGN.md')) note = '此页为目标系统设计，包含规划接口与历史边界。当前版本已接入 Replay；实际功能以玩家指南和对应实现文档为准。'
  if (source === 'docs/DESIGN.md') note = '此页保留架构记录，部分范围描述尚未追随最新版本：当前已包含四难度主线与独立 Extra。请结合逐关记录、难度接入与 Replay 文档阅读。'
  if (isCourse) note = '此页同步自 dev/koumakyou 的课程材料（2026 年 11 月 4 日），保留原文及其工作区、验证与规划边界。网站分支仅同步文档，未同步该提交的游戏实现；旧专项文档可能落后于课程材料。'
  const intro = `---\ntitle: ${JSON.stringify(title)}\n---\n\n::: info 来源与适用范围\n${note}\n\n[查看源文件](${site.repository}/blob/${sourceRevision}/${source}) · 来源版本 ${sourceRevision.slice(0, 7)}\n:::\n\n`
  await fs.mkdir(path.dirname(path.join(web, route)), { recursive: true })
  await fs.writeFile(path.join(web, route.replace('.html', '.md')), intro + body)
}
const history = parseHistory(git('log', '--date-order', '--format=%H%x00%aI%x00%an%x00%s%x00'), site.timezone)
// 核对引用后只生成关键事件和少量证据，避免拼错 SHA 或将摘要日期误作完整工期。
const events = milestones.map(event => {
  const evidence = event.refs.map(ref => {
    const matches = history.filter(entry => entry.hash.startsWith(ref))
    if (matches.length !== 1) throw new Error(`关键事件引用无效或不唯一：${ref}`)
    return { hash: matches[0].hash, date: matches[0].date }
  })
  const dates = evidence.map(entry => entry.date).sort()
  return { title: event.title, summary: event.summary, start: dates[0], end: dates.at(-1), evidence }
}).sort((a, b) => a.start.localeCompare(b.start))
await fs.writeFile(path.join(web, '_generated/milestones.json'), JSON.stringify(events))
await fs.writeFile(path.join(web, '_generated/meta.json'), JSON.stringify({ revision, groups: groups.map(([text, files]) => ({ text, collapsed: true, items: files.map(name => ({ text: titles[`docs/${name}.md`], link: `/reference/${name}` })) })) }, null, 2))
for (const [slug, title, needed] of coursePages) {
  await fs.mkdir(path.join(web, 'course'), { recursive: true })
  // 占位页只在首次创建时写入，后续人工补充的过程材料必须保留。
  const file = path.join(web, 'course', `${slug}.md`)
  try { await fs.access(file) } catch {
    await fs.writeFile(file, `# ${title}\n\n::: info 待补充\n本页为课程材料占位，尚未提供真实材料。\n:::\n\n需要补充：${needed}\n\n[返回课程材料](/course/) · [查看实际开发进度](/progress)\n`)
  }
}
for (const [source, dest] of [['title_layers/moon.png', 'moon.png'], ['title_layers/reimu.png', 'reimu.png']]) {
  await fs.copyFile(path.join(root, 'assets/runtime/common', source), path.join(web, 'public/media', dest))
}
console.log(`已生成 ${pages.size} 篇文档、${history.length} 条提交，版本 ${revision.slice(0, 7)}`)
