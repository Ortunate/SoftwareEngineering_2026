// 注册站点组件和阅读交互；事件在路由切换后仍由布局统一管理。
import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'
import Home from './Home.vue'
import Timeline from './Timeline.vue'
import Diagram from './Diagram.vue'
import Layout from './Layout.vue'
import Downloads from './Downloads.vue'
import './style.css'

export default {
  extends: DefaultTheme,
  Layout,
  enhanceApp({ app }) {
    app.component('ProjectHome', Home)
    app.component('Timeline', Timeline)
    app.component('Diagram', Diagram)
    app.component('Downloads', Downloads)
  }
} satisfies Theme
