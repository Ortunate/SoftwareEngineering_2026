// 覆盖静态检查无法发现的水合、中文搜索、筛选、图表及窄屏溢出问题。
import { test, expect } from '@playwright/test'
test('首页、移动布局与深层页面可直接访问', async ({ page }) => {
  const errors = []
  page.on('pageerror', error => errors.push(error.message))
  await page.goto('./')
  await expect(page).toHaveTitle(/东方红魔乡：异典/)
  await expect(page.locator('.VPNavBarTitle')).toContainText('东方红魔乡：异典')
  await expect(page.getByRole('heading', { level: 1 })).toContainText('弹幕的距离')
  for (const [width, height] of [[1440, 900], [1512, 982], [1920, 1080], [2560, 1080], [1024, 768], [820, 1180], [390, 844]]) {
    await page.setViewportSize({ width, height })
    const art = await page.locator('.hero-art').boundingBox()
    const copy = await page.locator('.hero-copy').boundingBox()
    const footer = await page.locator('.hero-bottom').boundingBox()
    expect(art.width).toBeGreaterThan(250)
    expect(art.x).toBeGreaterThanOrEqual(0)
    expect(art.x + art.width).toBeLessThanOrEqual(width)
    expect(art.y + art.height).toBeLessThanOrEqual(footer.y + 1)
    expect(art.x >= copy.x + copy.width - 1 || art.y >= copy.y + copy.height - 1).toBe(true)
    expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
  }
  await page.setViewportSize({ width: 1440, height: 900 })
  await page.screenshot({ path: '.vitepress/qa/home-desktop.png', fullPage: true })
  await page.setViewportSize({ width: 390, height: 844 })
  await expect.poll(() => page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
  await page.screenshot({ path: '.vitepress/qa/home-mobile.png', fullPage: true })
  await page.goto('guide/start.html')
  await expect(page.getByText('下载待提供', { exact: true })).toBeVisible()
  await page.reload()
  await expect(page.getByRole('heading', { level: 1 })).toContainText('获取与开始')
  await page.emulateMedia({ reducedMotion: 'reduce' })
  await page.setViewportSize({ width: 1280, height: 800 })
  await page.getByRole('link', { name: '开发进度', exact: true }).first().click()
  await expect(page.getByRole('heading', { level: 1 })).toContainText('开发进度')
  expect(await page.locator('.VPContent').evaluate(el => el.getAnimations().length)).toBe(0)
  expect(errors).toEqual([])
})
test('进度页展示关键事件与可追溯记录', async ({ page }) => {
  await page.goto('progress.html')
  await expect(page.getByRole('list', { name: '项目关键事件时间轴' })).toBeVisible()
  await expect(page.getByRole('heading', { name: '三维弹幕原型成形', exact: true })).toBeVisible()
  await expect(page.locator('.milestone-evidence a').first()).toHaveAttribute('href', /\/commit\/[a-f0-9]{40}$/)
  await expect(page.getByRole('button', { name: '下一页 →', exact: true })).toHaveCount(0)
  await page.setViewportSize({ width: 390, height: 844 })
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
})
test('中文搜索和 Mermaid 图表', async ({ page }) => {
  await page.goto('reference/SOFTWARE_DESIGN.html')
  await page.locator('.diagram-host').first().scrollIntoViewIfNeeded()
  await expect(page.locator('.diagram svg').first()).toBeVisible({ timeout: 30000 })
  for (const diagram of await page.locator('.diagram-host').all()) {
    await diagram.scrollIntoViewIfNeeded()
    await expect(diagram.locator('svg')).toBeVisible()
  }
  await page.screenshot({ path: '.vitepress/qa/document.png', fullPage: false })
  await page.getByRole('button', { name: '搜索文档', exact: true }).click()
  await page.locator('#localsearch-input').fill('录像')
  await expect(page.locator('.VPLocalSearchBox .results')).toContainText('录像')
  await page.keyboard.press('Escape')
  await expect(page.locator('#localsearch-input')).not.toBeVisible()
})

test('课程材料导航、来源与设计图表', async ({ page }) => {
  await page.goto('course/index.html')
  await page.getByRole('link', { name: '课程 NABCD', exact: true }).click()
  await expect(page.getByRole('heading', { level: 1 })).toContainText('NABCD')
  for (const name of ['NABCD', 'PROTOTYPE_SYSTEM_DESIGN', 'OUTLINE_DESIGN', 'DETAILED_DESIGN']) {
    await page.goto(`reference/course/${name}.html`)
    await expect(page.locator('.vp-doc a').filter({ hasText: '查看源文件' })).toHaveAttribute('href', new RegExp(`48216a7ad77a0531ce0f66518c486ef754d90b62/docs/course/${name}.md`))
    for (const diagram of await page.locator('.diagram-host').all()) {
      await diagram.scrollIntoViewIfNeeded()
      await expect(diagram.locator('svg')).toBeVisible()
    }
  }
})
