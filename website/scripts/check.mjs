// 校验静态产物中的站内页面、锚点和资源；不请求外部网络，失败时阻止部署。
import fs from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { load } from 'cheerio'
import { site } from '../site.config.mjs'
const web = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const dist = path.join(web, '.vitepress/dist')
async function walk(dir) {
  const result = []
  for (const entry of await fs.readdir(dir, { withFileTypes: true })) {
    const file = path.join(dir, entry.name)
    if (entry.isDirectory()) result.push(...await walk(file))
    else result.push(file)
  }
  return result
}
const files = await walk(dist)
const html = new Map()
for (const file of files.filter(f => f.endsWith('.html'))) html.set(file, load(await fs.readFile(file, 'utf8')))
const errors = new Set()
for (const [file, $] of html) {
  const sourceUrl = 'https://local.test' + site.base + path.relative(dist, file).replaceAll('\\', '/')
  for (const element of $('a[href], img[src], script[src], link[href]').toArray()) {
    const raw = $(element).attr('href') || $(element).attr('src')
    if (!raw || /^(?:https?:|mailto:|data:)/.test(raw)) continue
    const url = new URL(raw, sourceUrl)
    if (!url.pathname.startsWith(site.base)) { errors.add(`${file}: 部署路径缺失 ${raw}`); continue }
    let relative = decodeURIComponent(url.pathname.slice(site.base.length))
    if (!relative || relative.endsWith('/')) relative += 'index.html'
    let target = path.join(dist, relative)
    if (!files.includes(target) && !path.extname(target)) target += '.html'
    if (!files.includes(target)) { errors.add(`${path.relative(dist, file)}: 目标不存在 ${raw}`); continue }
    if (url.hash && html.has(target)) {
      const id = decodeURIComponent(url.hash.slice(1))
      if (!html.get(target)('[id]').toArray().some(e => html.get(target)(e).attr('id') === id)) errors.add(`${path.relative(dist, file)}: 锚点不存在 ${raw}`)
    }
  }
}
if (errors.size) { console.error([...errors].join('\n')); process.exitCode = 1 }
else console.log(`站内链接与资源检查通过：${html.size} 个页面`)
