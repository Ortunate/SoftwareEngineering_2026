'use strict';
(() => {
  const C = window.MathGarden;
  const STORAGE_KEY = 'savior.math-garden.v1';
  const $ = id => document.getElementById(id);
  let state = C.blankState();
  let active = null;
  let selectedPreset = 'starter';
  let loadProblem = false;
  let currentTab = 'practice';
  let dialogOpen = false;
  let printing = false;
  const expression = C.expression;
  const percentage = results => Math.round(results.filter(q => q.correct).length / results.length * 100);
  const dateLabel = date => new Date(date).toLocaleString('zh-CN', { year: 'numeric', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' });
  function notice(message) { $('notice').textContent = message; $('notice').hidden = false; }
  const duration = seconds => `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
  function syncTimer() {
    if (!active) return;
    active.timer.setRunning(!active.submitted && !document.hidden && currentTab === 'practice' && !dialogOpen && !printing);
    const clock = $('exercise-timer');
    if (clock) clock.textContent = duration(active.timer.seconds());
    const status = $('timer-status');
    if (status) status.textContent = active.submitted ? '已完成' : active.timer.isRunning() ? '练习用时' : '已暂停';
  }
  function confirmAction(message) {
    dialogOpen = true;
    syncTimer();
    try { return window.confirm(message); }
    finally { dialogOpen = false; syncTimer(); }
  }
  function performanceMarkup(session) {
    const evaluation = C.assess(session.results, session.seconds);
    const currentTiming = session.timing === 'active-v1';
    const label = currentTiming ? evaluation.label : session.timing === 'paper-v1' ? '纸笔练习记录' : '历史练习记录';
    const advice = currentTiming ? evaluation.advice : session.timing === 'paper-v1' ? '打印后的录入时间不代表纸笔作答时间，不作节奏评价。' : '这条旧记录的计时方式与当前版本不同，记录时长仅供查看，不用于判断熟练程度。';
    return `<div class="performance-card"><span class="pill">本次练习表现</span><h3>${label}</h3><div class="performance-metrics"><span>${currentTiming ? '练习用时' : '记录时长'} <strong>${duration(session.seconds)}</strong></span>${currentTiming ? `<span>平均 <strong>${evaluation.averageSeconds.toFixed(1)} 秒/题</strong></span>` : ''}<span>准确率 <strong>${evaluation.accuracy}%</strong></span></div><p>${advice}</p><details class="evaluation-rules"><summary>计时与判断规则</summary><p>准确率 = 答对题数 ÷ 总题数，空题计错；用时不影响准确率。平均用时包含本页检查答案的时间。练习计时在切换页面、隐藏标签、确认弹窗和打印时暂停。</p><p>全部作答且准确率 ≥ 90%，至少 5 题、平均用时 ≥ 1 秒时才参考节奏。每题参考：10 以内加减 8 秒、20 以内 12 秒、更大范围 20 秒，表内乘法 8 秒、整除除法 10 秒。高级题按步数累计，每步参考：100以内18秒、含负数20秒、千以内30秒、万以内45秒；范围较大时采用该范围的参考时间。当前题单参考均值为 ${evaluation.referenceSeconds.toFixed(1)} 秒/题。</p><p>这些是本应用的初始练习规则，不是教育测评标准。题目熟悉程度、输入方式和检查习惯均会影响用时；不用于不同孩子之间排名。</p></details></div>`;
  }
  function save(next) {
    state = next;
    try {
      if (loadProblem) throw new Error('Existing storage unreadable');
      localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
      return true;
    } catch {
      notice('记录暂时只保存在当前页面中，刷新可能丢失。请到「数据与帮助」导出备份；检查浏览器存储权限或剩余空间。');
      return false;
    }
  }
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) state = C.validateState(JSON.parse(raw));
  } catch {
    loadProblem = true;
    notice('本地记录无法读取，原数据未覆盖。你仍可练习并导出新记录；导入有效备份或确认清空后可重新保存。');
  }
  function tab(name) {
    if (!['practice', 'mistakes', 'history', 'settings'].includes(name)) name = 'practice';
    currentTab = name;
    syncTimer();
    document.querySelectorAll('.view').forEach(view => { view.hidden = view.id !== `${name}-view`; });
    document.querySelectorAll('[data-tab]').forEach(button => {
      button.classList.toggle('active', button.dataset.tab === name);
      if (button.dataset.tab === name) button.setAttribute('aria-current', 'page');
      else button.removeAttribute('aria-current');
    });
    renderSummaries();
  }
  function stat(number, title, subtitle) { return `<div class="stat-card"><span>${title}</span><strong>${number}</strong><small>${subtitle}</small></div>`; }
  function renderSummaries() {
    $('learning-progress').innerHTML = `<div class="card analysis-card"><h2>我的学习路线</h2><p class="hint">选择页面上的阶段练习后，这里会累计进度。每次至少10题，才用于判断是否适合进阶；积累满3次后，查看最近3次的正确率是否都达到90%。用时不影响进阶建议，你也可以随时选择其他阶段。仅调整题量仍计入所选阶段；修改其他条件则属于自定义练习。</p><div class="stage-grid">${Object.entries(C.PRESETS).map(([key, preset]) => {
      const p = C.progress(state, key);
      return `<article class="stage-card"><h3>${preset.label}</h3><p>${p.sessions ? `累计练习${p.sessions}次 · ${p.total}题 · 正确率${p.accuracy}%` : '暂无本阶段统计'}</p><small>${p.message}</small></article>`;
    }).join('')}</div><p class="hint">旧版记录没有标注阶段，仍可在下方查看；自定义练习和错题巩固也不计入这里。阶段累计持续保留，下方练习详情最多保留200次。负数属于选学内容。</p></div>`;
    const pending = state.mistakes.filter(m => m.streak < 2);
    $('mistake-badge').textContent = pending.length;
    const today = new Date().toDateString();
    const todays = state.sessions.filter(s => new Date(s.date).toDateString() === today);
    const todayResults = todays.flatMap(s => s.results);
    $('dashboard-stats').innerHTML = stat(todayResults.length, '今天已练习', '一道一道，积累进步') + stat(todayResults.length ? `${percentage(todayResults)}<em>%</em>` : '—', '今日正确率', todayResults.length ? '包含今日所有提交' : '完成练习后点亮') + stat(pending.length, '等待巩固', '把小失误变成新收获');
    $('review-button').disabled = !pending.length;
    const mastered = state.mistakes.length - pending.length;
    $('mistake-content').innerHTML = state.mistakes.length ? `<div class="mini-summary">${pending.length} 道待巩固 · ${mastered} 道已巩固</div><div class="mistake-grid">${[...state.mistakes].sort((a, b) => (a.streak >= 2) - (b.streak >= 2) || b.misses - a.misses).map(m => `<article class="card mistake-card"><span class="pill ${m.streak >= 2 ? 'success' : ''}">${m.streak >= 2 ? '已巩固' : `待巩固 · 原题已连续答对${m.streak}次，还需${2 - m.streak}次`}</span><h2>${expression(m)} = ${m.answer}</h2><p>${m.category} · 累计答错 ${m.misses} 次</p><small>上次错误答案：${m.lastAnswer === null ? '未填写或格式不正确' : m.lastAnswer}</small></article>`).join('')}</div>` : '<div class="empty-state"><span aria-hidden="true">✿</span><h2>花园里还没有错题</h2><p>完成一次练习后，需要巩固的题目会出现在这里。</p></div>';
    if (!state.sessions.length) {
      $('history-content').innerHTML = '<div class="empty-state"><span aria-hidden="true">▥</span><h2>第一步，从今天开始</h2><p>完成练习，留下第一份成长记录。</p></div>';
      return;
    }
    const all = state.sessions.flatMap(s => s.results);
    const categories = {};
    for (const q of all) { const item = categories[q.category] ||= { count: 0, wrong: 0 }; item.count++; if (!q.correct) item.wrong++; }
    $('history-content').innerHTML = `<div class="stat-grid">${stat(state.sessions.length, '已保存的练习', '仅统计下方保留的记录')}${stat(all.length, '已保存的题量', '包含普通练习与错题巩固')}${stat(`${percentage(all)}<em>%</em>`, '这些记录的正确率', '按已保存的所有题目计算')}</div><div class="card analysis-card"><h2>按题型看看掌握情况</h2>${Object.entries(categories).map(([label, item]) => `<div class="category-row"><span>${label}</span><div class="bar-track"><div style="width:${Math.round((item.count - item.wrong) / item.count * 100)}%"></div></div><span>${item.count - item.wrong}/${item.count} 正确</span></div>`).join('')}</div><div class="section-heading"><h2>每一次小小进步</h2><span class="hint">已保存 ${state.sessions.length} 次 · 最多保留 200 次</span></div><div class="history-list">${[...state.sessions].reverse().map(s => `<details class="card history-card"><summary><span><strong>${s.mode === 'review' ? '错题巩固' : s.stage ? C.PRESETS[s.stage].label : '普通练习（未标注阶段）'}</strong><small>${dateLabel(s.date)} · ${s.results.length} 题 · ${duration(s.seconds)}</small></span><span class="score-tag">${percentage(s.results)}%<small>正确率 · 展开详情</small></span></summary>${performanceMarkup(s)}<div class="result-grid">${s.results.map(q => resultCard(q)).join('')}</div></details>`).join('')}</div>`;
  }
  function presets() {
    $('preset-list').innerHTML = Object.entries(C.PRESETS).map(([key, preset], i) => `<button type="button" class="preset ${selectedPreset === key ? 'selected' : ''}" data-preset="${key}" aria-pressed="${selectedPreset === key}"><span class="preset-icon" aria-hidden="true">${['◔', '◑', '✳'][i % 3]}</span><span><strong>${preset.label}</strong><small>${preset.description}</small></span><span class="preset-check" aria-hidden="true">${selectedPreset === key ? '✓' : '○'}</span></button>`).join('');
  }
  function config() { return { max: Number($('range').value), count: Number($('count').value), carry: $('carry').value, mode: $('exercise-mode').value, steps: Number($('steps').value), facts: $('facts').value, negative: $('negative').checked, operations: [...document.querySelectorAll('[name="operation"]:checked')].map(input => input.value) }; }
  function canReplace() { return !active || active.submitted || confirmAction('当前练习尚未提交。放弃本次答案，开始新练习吗？'); }
  function start(questions, mode) {
    active = { id: `${Date.now()}-${Math.random().toString(36).slice(2)}`, questions, mode, stage: mode === 'practice' ? selectedPreset : null, timer: C.createTimer(), submitted: false, printed: false };
    $('setup-panel').hidden = true;
    $('exercise-panel').hidden = false;
    $('exercise-panel').innerHTML = `<div class="section-heading"><div><span class="eyebrow">ONE QUESTION AT A TIME</span><h1>${mode === 'review' ? '把错题变成进步' : '专心练习，慢慢来'}</h1><p>共 ${questions.length} 题 · 不限时，认真比速度更重要</p></div><button id="back-button" class="button secondary">重新设置</button></div><div class="exercise-toolbar"><span id="answer-progress" role="status" aria-live="polite">已填写 0 / ${questions.length}</span><button id="print-button" class="button secondary">打印题单与答案</button></div><form id="answer-form" novalidate><div class="question-grid">${questions.map((q, i) => `<div class="card question-card"><span class="question-number">${String(i + 1).padStart(2, '0')}</span><label for="answer-${i}">${expression(q)} =</label><input type="text" inputmode="text" maxlength="6" id="answer-${i}" data-question="${q.id}" aria-label="第 ${i + 1} 题：${expression(q)} 的答案" autocomplete="off"></div>`).join('')}</div><div class="submit-bar"><p>检查一遍，准备好就提交吧。</p><button class="button primary" type="submit">完成并批改 ✓</button></div></form><div id="result-panel" aria-live="polite"></div>`;
    $('answer-progress').insertAdjacentHTML('afterend', '<div class="timer-block"><span id="timer-status">练习用时</span><strong id="exercise-timer" role="timer" aria-label="练习用时">00:00</strong></div>');
    $('answer-form').insertAdjacentHTML('beforebegin', '<div class="keyboard-hints"><span><kbd>Tab</kbd> 下一题</span><span><kbd>Shift</kbd> + <kbd>Tab</kbd> 上一题</span><span><kbd>Enter</kbd> 提交批改</span><small>负数答案输入 -；可用草稿纸。切换页面或隐藏标签时暂停计时</small></div>');
    $('back-button').onclick = () => { if (canReplace()) { active = null; $('setup-panel').hidden = false; $('exercise-panel').hidden = true; renderSummaries(); } };
    $('print-button').onclick = printSheet;
    $('answer-form').oninput = () => { const filled = [...document.querySelectorAll('[data-question]')].filter(input => input.value.trim()).length; $('answer-progress').textContent = `已填写 ${filled} / ${questions.length}`; };
    $('answer-form').onsubmit = submit;
    tab('practice');
    $('answer-0').focus();
  }
  function resultCard(q) { return `<div class="result-item ${q.correct ? 'correct' : 'incorrect'}"><strong>${expression(q)} = ${q.answer}</strong><small>${q.correct ? '✓ 答对了' : `待巩固 · 你的答案 ${q.submitted === null ? '未填写或格式不正确' : q.submitted}`}</small>${q.kind === 'expression' ? `<details class="solution"><summary>查看计算步骤</summary><ol>${C.explain(q).map(step => `<li>${step}</li>`).join('')}</ol></details>` : ''}</div>`; }
  function submit(event) {
    event.preventDefault();
    if (!active || active.submitted) return;
    const inputs = [...document.querySelectorAll('[data-question]')];
    const invalid = inputs.find(input => input.value.trim() && C.parseAnswer(input.value) === null);
    if (invalid) { notice('答案请填写 -99999 至 99999 的整数，可使用负号，不支持小数或算式。'); invalid.focus(); return; }
    const blank = inputs.filter(input => !input.value.trim()).length;
    if (blank && !confirmAction(`还有 ${blank} 道题未填写，提交后将记为错题。确定提交吗？`)) return;
    const answers = Object.fromEntries(inputs.map(input => [input.dataset.question, input.value]));
    const results = C.grade(active.questions, answers);
    active.timer.setRunning(false);
    const session = { id: active.id, date: new Date().toISOString(), mode: active.mode, ...(active.stage ? { stage: active.stage } : {}), seconds: active.timer.seconds(), timing: active.printed ? 'paper-v1' : 'active-v1', results };
    save(C.record(state, session));
    active.submitted = true;
    syncTimer();
    inputs.forEach(input => { input.disabled = true; });
    $('answer-form').querySelector('[type="submit"]').disabled = true;
    const right = results.filter(q => q.correct).length;
    $('result-panel').innerHTML = `<div class="result-banner"><span class="eyebrow">WELL DONE</span><h2>${right === results.length ? '全部答对，给自己一个赞！' : '认真完成，就是很棒的一步。'}</h2><p>答对 ${right} / ${results.length} 题 · 正确率 ${percentage(results)}%</p><p>${right === results.length ? '可以休息一下，或换个范围继续探索。' : '看看下面的答案，再到错题花园巩固一下吧。'}</p><button id="result-review" class="button secondary">去错题花园 →</button></div><div class="result-grid">${results.map(resultCard).join('')}</div>`;
    $('result-review').onclick = () => tab('mistakes');
    $('result-panel').querySelector('.result-banner').insertAdjacentHTML('afterend', performanceMarkup(session));
    renderSummaries();
    $('result-panel').scrollIntoView({ behavior: 'smooth', block: 'start' });
  }
  function printSheet() {
    if (!active) return;
    active.printed = true;
    const questions = active.questions;
    $('print-area').innerHTML = `<section class="print-sheet"><h1>小芽口算 · 每日练习</h1><p>姓名：____________　日期：____________　共 ${questions.length} 题</p><div class="print-grid">${questions.map((q, i) => `<div>${i + 1}.　${expression(q)} = ______</div>`).join('')}</div></section><section class="print-sheet answer-sheet"><h1>小芽口算 · 参考答案</h1><p>请将本页与题单分开，由家长核对。</p><div class="print-grid">${questions.map((q, i) => `<div>${i + 1}.　${expression(q)} = ${q.answer}</div>`).join('')}</div></section>`;
    if (questions.some(q => q.kind === 'expression')) $('print-area').classList.add('advanced-print');
    else $('print-area').classList.remove('advanced-print');
    window.print();
  }
  document.querySelectorAll('[data-tab]').forEach(button => { button.onclick = () => tab(button.dataset.tab); });
  document.querySelector('.brand').onclick = event => { event.preventDefault(); tab('practice'); };
  $('preset-list').onclick = event => {
    const button = event.target.closest('[data-preset]');
    if (!button) return;
    selectedPreset = button.dataset.preset;
    const preset = C.PRESETS[selectedPreset];
    $('range').value = preset.max;
    $('carry').value = preset.carry;
    $('exercise-mode').value = preset.mode;
    $('steps').value = preset.steps;
    $('facts').value = preset.facts;
    $('negative').checked = preset.negative;
    syncOptions();
    document.querySelectorAll('[name="operation"]').forEach(input => { input.checked = preset.operations.includes(input.value); });
    presets();
  };
  function syncOptions() {
    const compound = $('exercise-mode').value !== 'single';
    $('steps').disabled = !compound;
    $('carry').disabled = compound || $('negative').checked;
    if ($('carry').disabled) $('carry').value = 'any';
  }
  $('config-form').onchange = event => { syncOptions(); if (event.target.id !== 'count') { selectedPreset = null; presets(); } };
  $('config-form').onsubmit = event => {
    event.preventDefault();
    try { const questions = C.generate(config()); if (canReplace()) start(questions, 'practice'); }
    catch (error) { notice(error.message); }
  };
  $('review-button').onclick = () => { if (canReplace()) { const questions = C.reviewQuestions(state); if (questions.length) start(questions, 'review'); } };
  $('export-button').onclick = () => {
    const blob = new Blob([JSON.stringify(state, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `math-garden-${new Date().toISOString().slice(0, 10)}.json`;
    anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    notice('已生成备份下载，请妥善保存 JSON 文件。');
  };
  $('import-file').onchange = async event => {
    const file = event.target.files[0];
    if (!file) return;
    try {
      if (file.size > 32 * 1024 * 1024) throw new Error('备份文件超过 32 MB，请选择本应用导出的备份。');
      const next = C.validateState(JSON.parse(await file.text()));
      if (!confirmAction(`备份包含 ${next.sessions.length} 次练习、${next.mistakes.length} 道错题。导入将替换本机记录并结束当前练习，继续吗？`)) return;
      loadProblem = false;
      if (save(next)) notice('备份导入完成。');
      active = null;
      $('setup-panel').hidden = false;
      $('exercise-panel').hidden = true;
      renderSummaries();
    } catch (error) { notice(error instanceof SyntaxError ? '文件不是有效的 JSON，原有记录未更改。' : error.message); }
    finally { event.target.value = ''; }
  };
  $('clear-button').onclick = () => {
    if (!confirmAction('确定清空全部练习记录和错题，并结束当前练习吗？此操作不能撤销。')) return;
    loadProblem = false;
    if (save(C.blankState())) notice('学习记录已清空，可以重新开始。');
    active = null;
    $('setup-panel').hidden = false;
    $('exercise-panel').hidden = true;
    renderSummaries();
  };
  window.addEventListener('beforeunload', event => { if (active && !active.submitted) { event.preventDefault(); event.returnValue = ''; } });
  document.addEventListener('visibilitychange', syncTimer);
  window.addEventListener('beforeprint', () => { printing = true; if (active) active.printed = true; syncTimer(); });
  window.addEventListener('afterprint', () => { printing = false; syncTimer(); });
  setInterval(syncTimer, 1000);
  window.addEventListener('storage', event => { if (event.key === STORAGE_KEY) notice('另一个页面更新了学习记录。请先导出本页备份，再刷新页面，以免互相覆盖。建议只在一个标签页练习。'); });
  $('today-label').textContent = new Date().toLocaleDateString('zh-CN', { month: 'long', day: 'numeric', weekday: 'long' });
  syncOptions();
  presets();
  tab('practice');
})();
