// 浏览验证使用本机 Edge；CI 使用 Playwright Chromium，统一验证构建产物而非开发服务器。
import { defineConfig } from '@playwright/test'
export default defineConfig({
  testDir: './browser-tests',
  use: { baseURL: 'http://127.0.0.1:4173/SoftwareEngineering_2026/', channel: process.platform === 'win32' ? 'msedge' : undefined, headless: true },
  webServer: { command: 'npm run preview -- --port 4173', url: 'http://127.0.0.1:4173/SoftwareEngineering_2026/', reuseExistingServer: !process.env.CI },
  reporter: 'list'
})
