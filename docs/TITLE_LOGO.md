# 首页标题字标

素材：`assets/runtime/common/title_logo_yidian.png`，2019×779 RGBA PNG。

使用内置 imagegen 生成并修订；参考为用户在 2026-10-12 提供的新典标题截图，随后按用户选择将题签改成“异典”。保留 Alpha，字标不包含月亮和云层，运行时叠加于现有插画背景。整张字标包含标题、英文副标题、装饰线及“异典”，避免分别缩放产生错位。以下记录完整生成和修订过程。

## 生成提示

```text
Use case: logo-brand. Asset: transparent PNG title logotype for a game main menu. Use the supplied image only as the typography/style reference. Create a polished standalone title logo closely following its composition and calligraphic lettering, on genuinely transparent alpha, wide aspect about 2.6:1. Exact top text: 东方红魔乡 (five legible Chinese characters in that order). Dramatic sharp tapered Song/Ming calligraphic strokes with elegant curved swashes, crimson/burgundy metallic gradients and fine light highlights. Make central 红 about 1.3 times larger and vivid scarlet, remaining characters deep burgundy. Beneath it, a centered ornate serif English subtitle EXACTLY: ~ the Embodiment of Scarlet Devil. Under the English line, a thin burgundy decorative divider with a tiny symmetrical filigree at the center and curled tips. Center the smaller Chinese text 新典 beneath the divider. Clear correct lettering is essential. Keep whole artwork within the canvas with only 3 percent transparent outer padding. No solid background, NO moon or cloud background (the game already has a moon), no opaque white rectangle, no mockup, no UI or scene, no extra text or watermark. This asset must blend over the project's existing illustrated menu background. Preserve transparent gaps inside letters and between all three title lines.
```

## 表面处理修订提示

Image 1 为首轮生成结果，Image 2 为用户参考。

```text
Edit Image 1, using Image 2 as the style reference. Preserve the lettering, arrangement, transparent background, larger central 红, English line, divider and 新典 exactly. Change ONLY the surface treatment: remove all outer red glow/noisy flecks, remove the heavy bevelled shiny chrome edges and silver outlines. Use the elegant nearly-flat smooth deep burgundy-to-scarlet fills of Image 2, with only tiny subtle interior highlight. Clean smooth anti-aliased edges, sharp calligraphic tips. The Chinese text must read 东方红魔乡 and 新典; English exactly ~ the Embodiment of Scarlet Devil. Real alpha transparency everywhere outside the lettering and thin ornament. No white backdrop, moon, background, additional text or effects. Keep the tight wide canvas.
```

## 异典题签最终修订提示

编辑上一版透明字标，只更改底部题签。

```text
Edit this transparent game title logo. Change ONLY the small two-character Chinese inscription at the bottom center, below the decorative divider, from 新典 to 异典. Render EXACTLY 异典 (simplified Chinese 异, then 典), in the same calligraphic Ming/Song style, burgundy gradient, stroke weight, size, baseline, centered position and decorative finish as the old inscription. Preserve everything else exactly: the large 东方红魔乡 title, its red central character, the English subtitle ~ the Embodiment of Scarlet Devil., the curled horizontal divider, spacing, dimensions, proportions, all colors and alpha transparency. Keep the original 2019×779 canvas if possible. No new elements, no moon, background or white rectangle. Actual transparent background, do not flatten alpha.
```
