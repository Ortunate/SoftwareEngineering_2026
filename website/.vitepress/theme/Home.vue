<!-- 项目首页使用已有标题素材；这些装饰图片不作为实机运行证据。 -->
<script setup lang="ts">
import { withBase } from 'vitepress'
const cards = [
  ['01', '进入三维弹幕', '在空间中观察、移动与躲避。第一人称视点即判定点，让每次擦弹都有新的距离感。', '/guide/play', '了解玩法'],
  ['02', '阅读实现细节', '从显式关卡时间轴到 120 Hz 行为更新，追踪游戏规则、资源与渲染如何协同。', '/development', '探索开发 Wiki'],
  ['03', '回看开发轨迹', '以真实 Git 提交记录项目演进，连接每一次功能接入、修复与性能改进。', '/progress', '查看开发进度']
]
</script>

<template>
  <main class="project-home">
    <section class="hero">
      <div class="hero-grid" aria-hidden="true"></div>
      <div class="hero-stage">
      <div class="hero-copy">
        <p class="eyebrow">东方红魔乡：异典 / PROJECT WIKI</p>
        <h1>红雾之中，<br>重新感受<span>弹幕的距离。</span></h1>
        <p class="hero-description">从二维轨迹到三维空间。<br>一场基于 C++20 与 raylib 的第一人称弹幕实践。</p>
        <div class="hero-actions"><a class="primary-action" :href="withBase('/guide/start.html')">开始探索 <span>↗</span></a><a class="secondary-action" :href="withBase('/development.html')">阅读开发 Wiki →</a></div>
        <p class="hero-caption">视觉素材：项目标题立绘与月亮图层 · 非实机截图</p>
      </div>
      <!-- 原图为 1672×941 且右侧透明；以固定素材坐标取景，再由外层等比缩放，避免窗口相关负偏移。 -->
      <svg class="hero-art" viewBox="0 0 1100 1100" role="img" aria-label="项目标题页灵梦立绘与红月" xmlns="http://www.w3.org/2000/svg">
        <image class="hero-art-moon" :href="withBase('/media/moon.png')" x="0" y="0" width="1550" height="872" />
        <svg class="hero-art-portrait" x="70" y="100" width="970" height="950" viewBox="0 0 970 941">
          <image :href="withBase('/media/reimu.png')" width="1672" height="941" />
        </svg>
      </svg>
      </div>
      <div class="hero-bottom"><span>东方红魔乡：异典 · 三维弹幕课设</span><span>WINDOWS / macOS</span></div>
    </section>
    <section class="home-intro"><p class="eyebrow">A DIFFERENT PERSPECTIVE</p><h2>熟悉的红雾，新的视角。</h2><p>保留关卡、角色与符卡的辨识度，在三轴移动与自由观察中探索新的躲避方式。这里记录游戏，也记录它如何被构建。</p></section>
    <section class="home-cards" aria-label="探索入口"><article v-for="card in cards" :key="card[0]"><span class="card-number">{{ card[0] }}</span><h3>{{ card[1] }}</h3><p>{{ card[2] }}</p><a :href="withBase(card[3] + '.html')">{{ card[4] }} ↗</a></article></section>
    <section class="home-strip"><div><p class="eyebrow">PLAY · PRACTICE · REPLAY</p><h2>一段完整旅程，也可以反复练习。</h2><p>四难度主线、独立 Extra、关卡与符卡练习，以及录像回看和音乐室。具体开放范围见使用指南。</p></div><a :href="withBase('/guide/features.html')">查看功能指南 →</a></section>
    <section class="home-course"><div><p class="eyebrow">BEHIND THE PROJECT</p><h2>从想法到实现</h2></div><div><p>已接入课程需求分析、原型系统设计、概要与详细设计。会议、计划、交互原型证据和用户反馈保留待补入口，真实开发进度可从提交历史追溯。</p><a :href="withBase('/course/index.html')">课程材料索引 →</a> <a :href="withBase('/sources.html')">素材与来源 ↗</a></div></section>
  </main>
</template>
