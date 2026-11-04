<!-- 展示经 Git 核对的关键事件及代表记录，不加载完整提交列表，不以提交数量表达工作量。 -->
<script setup lang="ts">
import events from '../../_generated/milestones.json'
import { site } from '../../site.config.mjs'
</script>
<template>
  <ol class="milestone-timeline" aria-label="项目关键事件时间轴">
    <li v-for="event in events" :key="event.title">
      <div class="milestone-date"><time :datetime="event.start">{{ event.start }}</time><template v-if="event.end !== event.start"><span>—</span><time :datetime="event.end">{{ event.end }}</time></template></div>
      <article><h2>{{ event.title }}</h2><p>{{ event.summary }}</p><div class="milestone-evidence"><span>相关记录</span><a v-for="(entry, index) in event.evidence" :key="entry.hash" :href="`${site.repository}/commit/${entry.hash}`" :aria-label="`${event.title}：查看相关提交 ${index + 1}`">{{ entry.hash.slice(0, 7) }} ↗</a></div></article>
    </li>
  </ol>
</template>
