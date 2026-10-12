# 首页背景分层

使用内置 imagegen，以原首页插画为风格及构图参考，分别生成红月、神社环境和博丽灵梦透明图层。灵梦随后按用户反馈修正托腮及搭在膝上的两只手。

运行时素材：

- `assets/runtime/common/title_layers/moon.png`：红月与半透明云雾。
- `assets/runtime/common/title_layers/shrine.png`：神社、石阶、鸟居与植被，无月亮或人物。
- `assets/runtime/common/title_layers/reimu.png`：修正手部后的独立人物，无座椅和环境。

三张 PNG 保留完整画布与 Alpha，以运行时坐标对齐；不把天空或标题烘焙进图层。绘制顺序为天空底色、红月、神社、灵梦、首页 UI；纹理由 AssetManager 持有并随通用资源释放。旧合成图仅作为素材缺失时的整页回退。

## 生成提示

### moon

```text
Create ONE production-ready transparent PNG layer for the illustrated main menu of 东方红魔乡：异典. The attached image is a STYLE AND COMPOSITION REFERENCE, not the output scene. Modern polished anime painting, delicate painterly lighting, soft ivory and vermilion/burgundy palette. Wide 16:9 canvas, ideally 2048x1152. All three project layers share a normalized composition: heroine seated on left foreground, moon upper left-of-center, shrine on left, quiet right third for menu. Preserve real alpha transparency outside the requested subject. Do not bake a solid sky or white background. No text, logos, UI, frames, title lettering or watermarks. This output must contain ONLY the requested layer. Requested layer: RED MOON ONLY with a small halo of soft translucent crimson cloud wisps immediately around it. Paint a beautiful round crimson full moon with detailed painterly crater texture and pale ivory limb, glowing softly. Moon center at x=34% of canvas width, y=30% of canvas height; diameter about 58% of canvas height. Keep circular shape and moon entirely within the canvas. Wisps fade smoothly into alpha. Leave the remaining canvas transparent, especially the right third. No shrine, buildings, steps, trees, flowers, person or character. Match the reference moon's warmth and soft texture.
```

### shrine

```text
Create ONE production-ready transparent PNG layer for the illustrated main menu of 东方红魔乡：异典. The attached image is a STYLE AND COMPOSITION REFERENCE, not the output scene. Modern polished anime painting, delicate painterly lighting, soft ivory and vermilion/burgundy palette. Wide 16:9 canvas, ideally 2048x1152. All three project layers share a normalized composition: heroine seated on left foreground, moon upper left-of-center, shrine on left, quiet right third for menu. Preserve real alpha transparency outside the requested subject. Do not bake a solid sky or white background. No text, logos, UI, frames, title lettering or watermarks. This output must contain ONLY the requested layer. Requested layer: SHRINE ENVIRONMENT ONLY, with no moon or characters. Red wooden Hakurei shrine eaves and pillars frame the upper left corner, carved wood, sacred rope and white shide; weathered stone steps descend diagonally from left toward lower center. A seat-height broad stone step is at x=17%, y=66%; steps beneath the heroine's eventual shoes run through x=35%, y=89%. Small vermilion maple/cherry foliage along the lower left border, another distant red torii and shrine roof in translucent mist at center-left. Fill the left half and lower foreground with this coherent environment, and let distant atmosphere fade to alpha. The upper middle open sky and rightmost 35% stay genuinely transparent and visually quiet. Complete clean edges for isolated architecture and stone; show material detail. The scene must work under a separately composited seated heroine. NO person, human, Reimu, silhouette, moon, large red sphere or sky background.
```

### reimu

```text
Create ONE production-ready transparent PNG layer for the illustrated main menu of 东方红魔乡：异典. The attached image is a STYLE AND COMPOSITION REFERENCE, not the output scene. Modern polished anime painting, delicate painterly lighting, soft ivory and vermilion/burgundy palette. Wide 16:9 canvas, ideally 2048x1152. All three project layers share a normalized composition: heroine seated on left foreground, moon upper left-of-center, shrine on left, quiet right third for menu. Preserve real alpha transparency outside the requested subject. Do not bake a solid sky or white background. No text, logos, UI, frames, title lettering or watermarks. This output must contain ONLY the requested layer. Requested layer: HAKUREI REIMU ONLY, fully isolated with actual alpha. Preserve the character identity and reference's seated thoughtful pose: long dark brown hair flowing left, crimson eyes, large red bow with white lace, red-and-white shrine maiden outfit, detached broad white sleeves with red trim, red skirt, white socks and dark red shoes. Sitting angled three-quarter toward viewer, one elbow on her knee and chin resting lightly on one hand, gentle confident expression. Full body and all bow/hair/shoes visible. Place her on LEFT of the wide canvas: occupied bounds approximately x=3%..40%, y=11%..93%; face center x=22%, y=32%; seat/hips around x=17%, y=65%; shoes extend to x=36%, y=89%. Right half entirely transparent. She is seated on an IMPLIED stone step but do not draw any seat, rock, steps or surface. Warm scarlet moon rim light from upper right, matching the shrine and moon reference. Clean anatomy, expressive illustration, tasteful detailed fabric. NO background, architecture, moon, stone, flowers, shadow platform or scenery.
```

## 手部修订提示

```text
Edit ONLY the two hands and their immediate wrist/sleeve junctions in this isolated transparent illustration of Hakurei Reimu. Repair the anatomy carefully. Hand 1 supports her chin gently: a coherent palm and wrist connecting naturally to the raised forearm, exactly five anatomically plausible digits, soft natural finger curl, no melted knuckles, fused fingers or impossible joints. Hand 2 rests loosely across the raised knee: palm down with a relaxed natural curve, one correctly placed thumb and four slender separate fingers with sensible lengths and joints, wrist attached correctly to its forearm and sleeve. Adjust tiny immediate surrounding skin/fabric only where needed for clean anatomical continuity. Preserve the face, expression, hair, bow, outfit, legs, shoes, seated pose, lighting, burgundy/ivory palette, exact canvas size, character position and all other pixels as closely as possible. No new objects or scenery. Preserve actual alpha transparency. The output is the complete corrected character layer, not a hand study or cropped image.
```

## 左手透视修订

用户确认是画面右侧的托腮左手，先修正透视，再改为掌面轻托下颌的手势。

```text
Localized anatomy correction of this full transparent Reimu character layer. Redraw her anatomical LEFT hand, which is the raised hand on the VIEWER'S RIGHT directly beneath her chin, together with its wrist and a small part of the forearm. The existing hand has an awkward flattened fist and wrong perspective: redesign it as a convincing relaxed three-quarter-view left hand gently supporting the chin. Back of hand faces viewer, palm turns toward her jaw; loosely curled fingers support the underside of the chin, correct metacarpal volume and overlapping knuckles, thumb lies naturally on the inner side and is partly occluded. Exactly five digits anatomically, but do NOT force all five to be visible; use natural foreshortening and occlusion. Wrist narrower than palm, smooth radius/ulna connection to the raised forearm, no broken wrist angle, no detached palm, no duplicated or fused fingers. Make the silhouette clearly different from the awkward current fist, with a subtle believable bend. Keep the elbow and relaxed seated pose. Preserve the knee-resting other hand and everything else: face/expression, hair, clothes, bow, body proportions, shoes, lighting, canvas composition and alpha. Output full isolated figure on actual transparent background, same 1672x941 canvas, no scenery, diagrams, captions or crop.
```

## 最终左手手势提示

```text
Redraw the raised chin-supporting LEFT HAND on viewer-right in this isolated Reimu illustration. IMPORTANT: replace the current closed fist with a DIFFERENT, clearly readable natural open cupping gesture, not another version of the same fist. Her relaxed left palm gently cups the underside of the jaw; thumb supports the inner underside of the chin, index finger bends softly beside the cheek, middle finger follows the jaw with believable foreshortening, ring and little finger naturally curl and are partly hidden. Back of hand in three-quarter view toward camera, five fingers anatomically but only the appropriately visible portions are drawn. Slender palm with real thickness, knuckle arc, clean finger overlap, believable wrist rotation and smooth connection to forearm. Adjust the immediate forearm and jaw-contact region only as necessary. This should look anatomically convincing at full resolution, not a flat skin blob, tiny detached fist, extra limb or broken wrist. Preserve face/expression, body, other hand on knee, all clothing, hair, legs, shoes, palette, lighting, original position and full wide canvas. Actual alpha background, no scene or labels. Output the entire corrected character layer at 1672x941.
```

红月的素材仍保持独立，运行时以画布内真实月球中心 (0.34,0.42) 对齐当前字标中心，不按透明画布中心或神社的位置对齐。
