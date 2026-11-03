<!-- 按香港日期筛选实际提交；作者为 Git 显示名，不代表已核实的团队成员身份。 -->
<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import entries from '../../_generated/history.json'
import { site } from '../../site.config.mjs'
const query = ref('')
const after = ref('')
const before = ref('')
const page = ref(1)
const filtered = computed(() => entries.filter(e => (!after.value || e.date >= after.value) && (!before.value || e.date <= before.value) && `${e.subject} ${e.author} ${e.hash}`.toLowerCase().includes(query.value.toLowerCase())))
const pages = computed(() => Math.max(1, Math.ceil(filtered.value.length / 20)))
const visible = computed(() => filtered.value.slice((page.value - 1) * 20, page.value * 20))
watch([query, after, before], () => { page.value = 1 })
</script>
<template>
  <div class="timeline-controls"><label>关键词<input v-model="query" type="search" placeholder="提交标题、作者或 SHA"></label><label>开始日期<input v-model="after" type="date"></label><label>结束日期<input v-model="before" type="date"></label></div>
  <p role="status">{{ filtered.length }} 条提交 · 第 {{ page }} / {{ pages }} 页</p>
  <p v-if="!visible.length">没有符合条件的提交，请调整筛选条件。</p>
  <ol class="timeline"><li v-for="entry in visible" :key="entry.hash"><time :datetime="entry.instant">{{ entry.date }}</time><div><a :href="`${site.repository}/commit/${entry.hash}`">{{ entry.subject }}</a><p>{{ entry.author }} <code>{{ entry.hash.slice(0, 7) }}</code></p></div></li></ol>
  <nav class="pagination" aria-label="提交分页"><button :disabled="page <= 1" @click="page--">← 上一页</button><button :disabled="page >= pages" @click="page++">下一页 →</button></nav>
</template>
