<!-- 仅对显式配置的发布包显示下载入口；空列表不会制造可下载的假象。 -->
<script setup>
import { site } from '../../site.config.mjs'
</script>
<template><div v-if="site.downloads.length"><p v-for="item in site.downloads" :key="item.url"><a :href="item.url">{{ item.platform }} · {{ item.version }} · {{ item.name }}</a></p></div><div v-else class="download-placeholder"><strong>下载待提供</strong><p>目前尚未配置已验证的公开运行包。可按下方说明从源码构建；发布包就绪后会在此列出版本、平台和获取地址。</p></div></template>
