// 验证网站最容易出现错误的子路径链接和跨时区历史解析，不依赖图形或网络。
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { resolveLink, parseHistory } from './content.mjs'
const site = { base: '/repo/', repository: 'https://github.com/owner/repo' }
test('跨目录文档、锚点和源码固定到正确版本', () => {
  const pages = new Map([['docs/B.md', 'reference/B.html']])
  assert.equal(resolveLink('B.md#接口', 'docs/A.md', pages, site, 'abc'), '/reference/B.html#接口')
  assert.equal(resolveLink('../src/main.cpp', 'docs/A.md', pages, site, 'abc'), 'https://github.com/owner/repo/blob/abc/src/main.cpp')
  assert.equal(resolveLink('https://example.com/a', 'docs/A.md', pages, site, 'abc'), 'https://example.com/a')
  assert.equal(resolveLink('C:/private/a.png', 'docs/A.md', pages, site, 'abc'), null)
})
test('Git 历史保留中文和特殊字符，日期转换为香港时间', () => {
  const rows = parseHistory('abc\x002026-11-02T18:00:00Z\x00作者\x00feat: 中文 | 标题\x00\ndef\x002026-11-01T00:00:00Z\x00另一作者\x00修复\x00', 'Asia/Hong_Kong')
  assert.equal(rows.length, 2)
  assert.equal(rows[0].date, '2026-11-03')
  assert.equal(rows[0].subject, 'feat: 中文 | 标题')
  assert.equal(rows[1].hash, 'def')
  assert.equal('email' in rows[0], false)
})
