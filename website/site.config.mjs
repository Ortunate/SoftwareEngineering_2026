// 集中定义网站来源、部署路径和下载信息；下载条目仅填写已发布的真实安装包。
export const site = {
  name: '红雾幻想',
  subtitle: 'koumakyou · 三维弹幕计划',
  repository: 'https://github.com/Ortunate/SoftwareEngineering_2026',
  branch: 'dev/koumakyou',
  base: '/SoftwareEngineering_2026/',
  timezone: 'Asia/Hong_Kong',
  downloads: []
}

// 每组列出的文件来自 docs/；顺序同时决定文档导航，遗漏文件由生成器报错。
export const groups = [
  ['需求与架构', ['NABCD', 'DESIGN', 'SOFTWARE_DESIGN', 'TH06NC_APP']],
  ['关卡与叙事', ['STAGE_PORTING_METHOD', 'STAGE1_BEHAVIORS', 'BOSS_START_CONFIG', 'STAGE1_REFERENCE', 'STAGE1_DIALOGUE_REFERENCE', 'STAGES_2_6_REFERENCE', 'STAGE2_REFERENCE', 'STAGE3_REFERENCE', 'STAGE4_REFERENCE', 'STAGE5_REFERENCE', 'STAGE6_REFERENCE', 'STAGE_EXTRA_REFERENCE', 'ENDING_REFERENCE', 'DIFFICULTY_INTEGRATION', 'DIFFICULTY_DEFERRED_AUDIT', 'SPELL_PRACTICE_REFERENCE']],
  ['战斗与弹体', ['PROJECTILE_RUNTIME', 'PROJECTILE_CATALOG', 'PROJECTILE_DIRECTION', 'PROJECTILE_LASERS', 'LASER_RENDERING', 'PROJECTILE_REIMU', 'PROJECTILE_MARISA', 'PROJECTILE_OPTIONS', 'PROJECTILE_BOMBS', 'PROJECTILE_EFFECTS', 'PLAYER_BOMBS_REFERENCE', 'REIMU_HOMING_REFERENCE', 'REIMU_NEEDLE_REFERENCE', 'MARISA_SHOTS_REFERENCE', 'ITEM_MODELS', 'NATIVE_COMBAT_DATA', 'NATIVE_DIFFICULTY_EVIDENCE', 'NATIVE_EFFECTS_EVIDENCE', 'NATIVE_SIZES']],
  ['界面与录像', ['UI_MODULE', 'UI_PRESENTATION', 'START_CHARACTER_SELECTION', 'SETTINGS_UPDATE', 'REPLAY', 'REPLAY_PROTOCOL']],
  ['资源与制作', ['PROJECTILE_MODELING_METHOD', 'PROJECTILE_COLLECTION_REVIEW', 'TITLE_LOGO', 'TITLE_BACKGROUND_LAYERS', 'STAGE_PORTRAITS', 'STAGE1_ASSETS', 'STAGE_EXTRA_ASSETS', 'NATIVE_BATTLE_ASSETS']],
  ['性能与验证', ['RENDER_PERFORMANCE_ANALYSIS', 'RENDER_OPTIMIZATION_RESULTS', 'PROJECTILE_COLLECTION_PERFORMANCE']]
]

export const coursePages = [
  ['initiation', '立项与会议', '参会者、角色、真实时间、讨论结论和会议照片。'],
  ['mindmap', '头脑风暴与思维导图', '候选方案、选择依据及实际使用的思维导图。'],
  ['prototype', '交互原型', '可访问的原型地址、完整交互流程、截图与制作时间。'],
  ['team', '成员与分工', '真实成员、职责、交付物及可核对的贡献记录。'],
  ['schedule', '任务计划与甘特图', '不少于 30 项任务、负责人、计划起止时间和甘特图；原计划与实际复盘分别标注。'],
  ['feedback', '用户反馈', '真实使用记录、体验反馈、照片或视频，以及后续处理。'],
  ['report', '项目报告与个人体会', '最终项目报告及每位成员结合实际分工撰写的个人体会。']
]
