// 为文档生成器提供纯函数；Markdown 页面使用站点根路径，部署前缀由 VitePress 添加。
import path from 'node:path'

/** 将相对仓库链接解析为站内页面或固定提交的源码地址；本机绝对路径不生成超链接。 */
export function resolveLink(href, source, pages, site, revision) {
  if (/^(?:https?:|mailto:|#|data:)/i.test(href)) return href
  if (/^(?:file:|[A-Za-z]:[\\/]|\/Users\/|\/tmp\/)/.test(href)) return null
  const [file, fragment] = href.split('#')
  const target = path.posix.normalize(path.posix.join(path.posix.dirname(source), decodeURI(file)))
  const suffix = fragment ? `#${fragment}` : ''
  if (pages.has(target)) return '/' + pages.get(target) + suffix
  return `${site.repository}/blob/${revision}/${target.split('/').map(encodeURIComponent).join('/')}${suffix}`
}

/** 解析 NUL 分隔的 Git 字段；输入仅含 hash、ISO 日期、显示名与标题，不读取邮箱。 */
export function parseHistory(raw, timezone) {
  const values = raw.split('\0')
  const entries = []
  for (let i = 0; i + 3 < values.length; i += 4) {
    const hash = values[i].trim()
    if (!hash) continue
    const instant = values[i + 1]
    entries.push({ hash, instant, date: new Intl.DateTimeFormat('en-CA', { timeZone: timezone, year: 'numeric', month: '2-digit', day: '2-digit' }).format(new Date(instant)), author: values[i + 2], subject: values[i + 3] })
  }
  return entries
}
