<!-- 扩展默认布局，提供版本来源、面包屑和可用 Escape 关闭的正文图片查看器。 -->
<script setup lang="ts">
import DefaultTheme from 'vitepress/theme'
import { useData, withBase } from 'vitepress'
import { onMounted, onUnmounted, ref } from 'vue'
import meta from '../../_generated/meta.json'
import { site } from '../../site.config.mjs'
const { page } = useData()
const dialog = ref<HTMLDialogElement>()
const image = ref({ src: '', alt: '' })
let opener: HTMLElement | null = null
function show(event: MouseEvent) {
  const target = event.target as HTMLElement
  if (target instanceof HTMLImageElement && target.closest('.vp-doc')) {
    opener = target
    image.value = { src: target.src, alt: target.alt }
    dialog.value?.showModal()
  }
}
function close() { dialog.value?.close(); opener?.focus() }
onMounted(() => document.addEventListener('click', show))
onUnmounted(() => document.removeEventListener('click', show))
</script>
<template><DefaultTheme.Layout><template #doc-before><nav class="breadcrumbs" aria-label="当前位置"><a :href="withBase('/')">首页</a><span> / {{ page.title }}</span></nav></template><template #doc-after><div class="source-footer">文档构建来源 <a :href="`${site.repository}/commit/${meta.revision}`">{{ meta.revision.slice(0, 7) }}</a> · <a :href="withBase('/sources.html')">素材与来源</a></div></template></DefaultTheme.Layout><dialog ref="dialog" class="image-dialog" @click="close"><button @click.stop="close" autofocus>关闭 ×</button><img :src="image.src || undefined" :alt="image.alt"></dialog></template>
