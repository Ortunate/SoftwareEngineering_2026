// 图表共享一个初始化实例并串行渲染，避免长文档同时创建多个布局引擎。
let engine: Promise<typeof import('mermaid')['default']> | undefined
let queue: Promise<unknown> = Promise.resolve()
/** source 为 Mermaid 定义，id 在当前页面唯一；失败会拒绝且不阻断后续图表。 */
export function renderDiagram(id: string, source: string): Promise<string> {
  engine ??= import('mermaid').then(({ default: mermaid }) => {
    mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'neutral', suppressErrorRendering: true })
    return mermaid
  })
  const task = queue.then(async () => (await (await engine!).render(id, source)).svg)
  queue = task.catch(() => undefined)
  return task
}
