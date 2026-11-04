<!-- Mermaid 仅在客户端按需加载；失败时展示原始图定义，避免整页不可用。 -->
<script setup lang="ts">
import { onMounted, onUnmounted, ref, useId } from 'vue'
import { renderDiagram } from './diagrams'
const props = defineProps<{ source: string }>()
const svg = ref('')
const failed = ref(false)
const source = decodeURIComponent(props.source)
const id = 'diagram-' + useId().replace(/[^a-z0-9]/gi, '')
const host = ref<HTMLElement>()
let observer: IntersectionObserver | undefined
async function render() {
  try {
    svg.value = await renderDiagram(id, source)
  } catch { failed.value = true }
}
onMounted(() => {
  observer = new IntersectionObserver(entries => {
    if (entries.some(entry => entry.isIntersecting)) { observer?.disconnect(); void render() }
  }, { rootMargin: '200px' })
  if (host.value) observer.observe(host.value)
})
onUnmounted(() => observer?.disconnect())
</script>
<template><div ref="host" class="diagram-host"><div class="diagram" v-if="svg" v-html="svg"></div><details v-else :open="failed"><summary>{{ failed ? '图表渲染失败，查看定义' : '图表定义（进入视野后加载）' }}</summary><pre>{{ source }}</pre></details></div></template>
