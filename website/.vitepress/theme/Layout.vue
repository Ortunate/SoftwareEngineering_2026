<!-- 扩展默认布局，提供版本来源、面包屑、页面入场过渡和正文图片查看器。 -->
<script setup lang="ts">
import DefaultTheme from 'vitepress/theme'
import { useData, withBase } from 'vitepress'
import { onMounted, onUnmounted, ref, watch } from 'vue'
import meta from '../../_generated/meta.json'
import { site } from '../../site.config.mjs'
const { page } = useData()
const dialog = ref<HTMLDialogElement>()
const image = ref({ src: '', alt: '' })
let opener: HTMLElement | null = null
let transition: Animation | undefined
// 仅对新页面做短入场，不延迟导航；连续导航必须取消旧动画，减少动态效果时不播放。
function enterPage() {
  transition?.cancel()
  if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return
  transition = document.querySelector('.VPContent')?.animate(
    [{ opacity: 0, transform: 'translateY(8px)' }, { opacity: 1, transform: 'translateY(0)' }],
    { duration: 260, easing: 'cubic-bezier(.2,.7,.3,1)' }
  )
}
watch(() => page.value.relativePath, enterPage, { flush: 'post' })
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
onUnmounted(() => { document.removeEventListener('click', show); transition?.cancel() })
</script>
<template><DefaultTheme.Layout><template #doc-before><nav class="breadcrumbs" aria-label="当前位置"><a :href="withBase('/')">首页</a><span> / {{ page.title }}</span></nav></template><template #doc-after><div class="source-footer">文档构建来源 <a :href="`${site.repository}/commit/${meta.revision}`">{{ meta.revision.slice(0, 7) }}</a> · <a :href="withBase('/sources.html')">素材与来源</a></div></template></DefaultTheme.Layout><dialog ref="dialog" class="image-dialog" @click="close"><button @click.stop="close" autofocus>关闭 ×</button><img :src="image.src || undefined" :alt="image.alt"></dialog></template>
