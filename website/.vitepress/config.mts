// VitePress 构建配置；导入文档由生成器转义，站点手写页面允许使用 Vue 组件。
import { defineConfig } from 'vitepress'
import { site, coursePages } from '../site.config.mjs'
import meta from '../_generated/meta.json'

export default defineConfig({
  lang: 'zh-CN', title: site.name, description: 'koumakyou 三维弹幕游戏 · 项目官网与开发 Wiki',
  base: site.base, cleanUrls: false,
  srcExclude: ['README.md', 'test-results/**', 'playwright-report/**', 'browser-tests/**', 'scripts/**'],
  appearance: 'light', lastUpdated: false,
  head: [['meta', { name: 'theme-color', content: '#9e304a' }]],
  markdown: {
    html: true,
    config(md) {
      const original = md.renderer.rules.fence!
      md.renderer.rules.fence = (tokens, idx, options, env, self) => {
        if (tokens[idx].info.trim() === 'mermaid') {
          return `<Diagram source="${encodeURIComponent(tokens[idx].content)}" />`
        }
        return original(tokens, idx, options, env, self)
      }
    }
  },
  themeConfig: {
    siteTitle: site.name,
    nav: [{ text: '开始游戏', link: '/guide/start' }, { text: '开发 Wiki', link: '/development' }, { text: '开发进度', link: '/progress' }, { text: '课程材料', link: '/course/' }],
    sidebar: {
      '/guide/': [{ text: '玩家指南', items: [{ text: '获取与开始', link: '/guide/start' }, { text: '玩法与模式', link: '/guide/play' }, { text: '录像、音乐与设置', link: '/guide/features' }] }],
      '/reference/': meta.groups,
      '/course/': [{ text: '课程材料', items: [{ text: '材料索引', link: '/course/' }, ...coursePages.map(([slug, title]) => ({ text: title, link: `/course/${slug}` }))] }],
      '/development': meta.groups,
      '/testing': meta.groups,
      '/sources': meta.groups
    },
    search: { provider: 'local', options: { locales: { root: { translations: { button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' }, modal: { noResultsText: '没有找到结果', resetButtonTitle: '清除搜索', footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' } } } } } } },
    outline: { label: '本页目录', level: [2, 3] },
    docFooter: { prev: '上一篇', next: '下一篇' },
    sidebarMenuLabel: '文档导航', returnToTopLabel: '返回顶部', darkModeSwitchLabel: '外观',
    socialLinks: [{ icon: 'github', link: site.repository }],
    footer: { message: `项目 Wiki · 构建来源 ${meta.revision.slice(0, 7)}`, copyright: '课程实践项目 · 素材来源见来源说明' }
  }
})
