// 覆盖静态检查无法发现的水合、中文搜索、筛选、图表及窄屏溢出问题。
import { test, expect } from '@playwright/test'
test('首页、移动布局与深层页面可直接访问', async ({ page }) => {
  const errors = []
  page.on('pageerror', error => errors.push(error.message))
  await page.goto('./')
  await expect(page).toHaveTitle(/东方红魔乡：异典/)
  await expect(page.locator('.VPNavBarTitle')).toContainText('东方红魔乡：异典')
  await expect(page.getByRole('heading', { level: 1 })).toContainText('弹幕的距离')
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
test('进度页筛选与分页', async ({ page }) => {
  await page.goto('progress.html')
  await expect(page.locator('.timeline li')).toHaveCount(20)
  await page.getByRole('button', { name: '下一页 →', exact: true }).click()
  await expect(page.getByRole('status')).toContainText('第 2')
  await page.getByLabel('关键词', { exact: true }).fill('music')
  await expect(page.getByRole('status')).toContainText('第 1')
  await expect(page.locator('.timeline')).toContainText('music')
  await page.getByLabel('关键词', { exact: true }).fill('不存在的提交XYZ')
  await expect(page.getByText('没有符合条件的提交，请调整筛选条件。')).toBeVisible()
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
