(function (root, factory) {
  const api = factory(typeof module === 'object' && module.exports ? require('./learning.js') : root.MathLearning);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MathGarden = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function (L) {
  'use strict';

  const PRESETS = L.PRESETS;
  const SYMBOLS = { '+': '+', '-': '−', '*': '×', '/': '÷' };
  const blankState = () => ({ version: 2, sessions: [], mistakes: [], stageStats: {} });
  const validateConfig = L.normalize;
  const canonicalQuestion = q => q.kind === 'expression' ? L.make(q.tree, q.profile) : makeQuestion(q.a, q.b, q.op);
  const expression = q => q.kind === 'expression' ? L.expression(q.tree) : `${q.a} ${SYMBOLS[q.op]} ${q.b}`;
  function parseAnswer(raw) {
    const text = String(raw ?? '').trim().replace(/[−－]/g, '-').replace(/[０-９]/g, x => String(x.charCodeAt(0) - 65296));
    return /^-?\d{1,5}$/.test(text) ? Number(text) : null;
  }
  function progress(state, key) {
    const item = state.stageStats?.[key];
    const recent = item?.recentAccuracy || [];
    const sessions = item?.sessions || 0;
    const sampleCount = recent.length;
    const qualified = recent.filter(x => x >= .9).length;
    const ready = sampleCount === 3 && qualified === 3;
    let message;
    if (!sessions) message = '暂无本阶段记录。选择这个阶段完成练习后，这里会显示进度。';
    else if (!sampleCount) message = '已有练习记录，但每次都不足10题。完成一套至少10题的练习后，开始积累进阶参考。';
    else if (sampleCount < 3) message = `已有${sampleCount}次可用于进阶参考的练习，其中${qualified}次正确率达到90%。还需${3 - sampleCount}次至少10题的练习，再判断是否适合进阶。`;
    else if (ready) message = '最近3次至少10题的练习，正确率都达到90%。可以尝试更有挑战的内容，也可以继续巩固。';
    else message = `最近3次至少10题的练习中，${qualified}次正确率达到90%。建议先巩固错题；这3次都达到90%时，再考虑进阶。`;
    return { sessions, total: item?.total || 0, accuracy: item?.total ? Math.round(item.correct / item.total * 100) : null, ready, qualified, sampleCount, message };
  }
  function makeQuestion(a, b, op) {
    const answer = op === '+' ? a + b : op === '-' ? a - b : op === '*' ? a * b : a / b;
    const carry = op === '+' ? a % 10 + b % 10 >= 10 : op === '-' ? a % 10 < b % 10 : false;
    const category = op === '+' ? (carry ? '进位加法' : '不进位加法') : op === '-' ? (carry ? '退位减法' : '不退位减法') : op === '*' ? '表内乘法' : '整除除法';
    return { id: `${a}:${op}:${b}`, a, b, op, answer, category };
  }
  function buildPool(config) {
    config = validateConfig(config);
    if (config.max > 100 || config.mode !== 'single' || config.negative || config.facts !== 'table') throw new Error('高级题型使用按需生成。');
    const pool = [];
    for (const op of config.operations) {
      for (let a = 0; a <= config.max; a++) {
        for (let b = 0; b <= config.max; b++) {
          if (op === '+' && a + b > config.max) continue;
          if (op === '-' && a < b) continue;
          if (op === '*' && (a < 1 || b < 1 || a > 9 || b > 9 || a * b > config.max)) continue;
          if (op === '/' && (a < 1 || b < 1 || b > 9 || a % b !== 0 || a / b > 9)) continue;
          const q = makeQuestion(a, b, op);
          if ((op === '+' || op === '-') && config.carry !== 'any') {
            const carries = q.category === '进位加法' || q.category === '退位减法';
            if (carries !== (config.carry === 'with')) continue;
          }
          pool.push(q);
        }
      }
    }
    return pool;
  }
  function shuffle(items, random = Math.random) {
    const output = [...items];
    for (let i = output.length - 1; i > 0; i--) {
      const j = Math.floor(random() * (i + 1));
      [output[i], output[j]] = [output[j], output[i]];
    }
    return output;
  }
  function generate(config, random = Math.random) {
    config = validateConfig(config);
    if (config.max > 100 || config.mode !== 'single' || config.negative || config.facts !== 'table') return L.generate(config, random);
    const pool = buildPool(config);
    if (pool.length < config.count) throw new Error(`当前条件只有 ${pool.length} 道不同题目，请减少题量或放宽范围。`);
    // Round-robin across selected operations avoids a large addition pool drowning out division.
    const groups = config.operations.map(op => shuffle(pool.filter(q => q.op === op), random));
    const selected = [];
    while (selected.length < config.count) {
      for (const group of groups) {
        if (group.length && selected.length < config.count) selected.push(group.pop());
      }
    }
    return shuffle(selected, random);
  }
  function grade(questions, answers) {
    return questions.map(q => {
      const raw = String(answers[q.id] ?? '').trim();
      const value = parseAnswer(raw);
      return { ...q, submitted: value, correct: value === q.answer };
    });
  }
  function record(state, session) {
    const next = JSON.parse(JSON.stringify(state));
    if (next.sessions.some(s => s.id === session.id)) return next;
    next.version = 2;
    next.stageStats ||= {};
    if (session.mode === 'practice' && Object.hasOwn(PRESETS, session.stage)) {
      const item = next.stageStats[session.stage] ||= { sessions: 0, total: 0, correct: 0, recentAccuracy: [] };
      const correct = session.results.filter(q => q.correct).length;
      item.sessions++; item.total += session.results.length; item.correct += correct;
      if (session.results.length >= 10) item.recentAccuracy = [...item.recentAccuracy, correct / session.results.length].slice(-3);
    }
    next.sessions.push(session);
    next.sessions = next.sessions.slice(-200);
    for (const q of session.results) {
      const old = next.mistakes.find(m => m.id === q.id);
      if (!q.correct) {
        if (old) Object.assign(old, { misses: old.misses + 1, streak: 0, lastSeen: session.date, lastAnswer: q.submitted });
        else next.mistakes.push({ ...canonicalQuestion(q), misses: 1, streak: 0, lastSeen: session.date, lastAnswer: q.submitted });
      } else if (old) {
        old.streak = Math.min(2, old.streak + 1);
        old.lastSeen = session.date;
      }
    }
    next.mistakes = next.mistakes.slice(-2000);
    return next;
  }
  function reviewQuestions(state, random = Math.random) {
    const pending = state.mistakes.filter(m => m.streak < 2);
    if (!pending.length) return [];
    // Original questions verify mastery; same-category variants prevent rote answer recall.
    const originals = shuffle(pending, random).slice(0, 10).map(canonicalQuestion);
    const selected = [...originals];
    const ids = new Set(selected.map(q => q.id));
    const pool = buildPool({ max: 100, count: 30, operations: ['+', '-', '*', '/'], carry: 'any' });
    for (const old of originals) {
      if (old.kind === 'expression') {
        try {
          const variant = L.generate({ ...old.profile, count: 5 }, random).find(q => q.category === old.category && !ids.has(q.id));
          if (variant) { selected.push(variant); ids.add(variant.id); }
        } catch { /* Keep the original when no valid variant can be generated. */ }
        continue;
      }
      const ceiling = Math.max(old.a, old.b, old.answer);
      const range = [10, 20, 50, 100].find(n => n >= ceiling);
      const candidates = pool.filter(q => q.category === old.category && Math.max(q.a, q.b, q.answer) <= range && !ids.has(q.id));
      if (candidates.length) {
        const q = candidates[Math.floor(random() * candidates.length)];
        selected.push(q);
        ids.add(q.id);
      }
    }
    return shuffle(selected, random);
  }
  function createTimer(now = () => performance.now()) {
    let accumulated = 0;
    let runningSince = null;
    return {
      setRunning(running) {
        const time = now();
        if (runningSince !== null) accumulated += Math.max(0, time - runningSince);
        runningSince = running ? time : null;
      },
      seconds() {
        const extra = runningSince === null ? 0 : Math.max(0, now() - runningSince);
        return Math.floor((accumulated + extra) / 1000);
      },
      isRunning() { return runningSince !== null; }
    };
  }
  function assess(results, seconds) {
    if (!results.length || !Number.isFinite(seconds) || seconds < 0) throw new Error('练习评估数据无效。');
    const correct = results.filter(q => q.correct).length;
    const answered = results.filter(q => q.submitted !== null).length;
    const ratio = correct / results.length;
    const referenceTotal = results.reduce((sum, q) => sum + L.referenceSeconds(q), 0);
    let tier, label, advice;
    if (answered < results.length) {
      tier = 'incomplete'; label = '先完成，再看节奏'; advice = '本次有未填写的题目，不判断熟练度。先把空题和错题补练一下。';
    } else if (ratio < 0.9) {
      tier = 'accuracy'; label = '先巩固准确性'; advice = '先关注错题的计算方法，暂时不用追求更快。';
    } else if (results.length < 5 || seconds < results.length) {
      tier = 'insufficient'; label = '暂不判断熟练度'; advice = '本次题量或计时不足以作为节奏参考，保留正确率和用时记录。';
    } else if (seconds <= referenceTotal) {
      tier = 'fluent'; label = ratio === 1 ? '正确又顺畅' : '较熟练，注意小失误'; advice = ratio === 1 ? '本套题正确且节奏流畅，可以保持练习，或尝试稍高一点的难度。' : '本套题节奏流畅，先订正少量错题，再继续练习。';
    } else {
      tier = 'steady'; label = '准确优先，稳步熟悉'; advice = '准确率已经不错。按自己的节奏巩固，逐步减少犹豫即可。';
    }
    return { tier, label, advice, accuracy: Math.round(ratio * 100), averageSeconds: seconds / results.length, referenceSeconds: referenceTotal / results.length };
  }
  function validateState(data) {
    const fail = () => { throw new Error('备份格式不正确，原有记录未更改。'); };
    if (!data || ![1, 2].includes(data.version) || !Array.isArray(data.sessions) || !Array.isArray(data.mistakes) || data.sessions.length > 200 || data.mistakes.length > 2000) fail();
    const question = q => {
      if (q?.kind === 'expression') {
        let canonical;
        try { canonical = L.make(q.tree, q.profile); } catch { fail(); }
        if (q.id !== canonical.id || q.answer !== canonical.answer || q.category !== canonical.category) fail();
        return;
      }
      if (!q || !Number.isInteger(q.a) || !Number.isInteger(q.b) || q.a < 0 || q.b < 0 || q.a > 100 || q.b > 100 || !Object.hasOwn(SYMBOLS, q.op) || (q.op === '/' && (q.a < 1 || q.b < 1 || q.b > 9 || q.a / q.b > 9)) || (q.op === '*' && (q.a < 1 || q.b < 1 || q.a > 9 || q.b > 9))) fail();
      const canonical = makeQuestion(q.a, q.b, q.op);
      if (q.id !== canonical.id || q.answer !== canonical.answer || q.category !== canonical.category || !Number.isInteger(q.answer) || q.answer < 0 || q.answer > 100) fail();
    };
    const validDate = x => typeof x === 'string' && x.length <= 40 && Number.isFinite(Date.parse(x));
    const submitted = x => x === null || (Number.isInteger(x) && x >= -99999 && x <= 99999);
    const sessionIds = new Set();
    for (const s of data.sessions) {
      if (!s || typeof s.id !== 'string' || s.id.length > 100 || sessionIds.has(s.id) || !validDate(s.date) || !['practice', 'review'].includes(s.mode) || !Number.isInteger(s.seconds) || s.seconds < 0 || !Array.isArray(s.results) || s.results.length < 1 || s.results.length > 50) fail();
      if (s.timing !== undefined && !['active-v1', 'paper-v1'].includes(s.timing)) fail();
      if (s.stage !== undefined && !Object.hasOwn(PRESETS, s.stage)) fail();
      sessionIds.add(s.id);
      const ids = new Set();
      for (const q of s.results) {
        question(q);
        if (ids.has(q.id) || !submitted(q.submitted) || q.correct !== (q.submitted === q.answer)) fail();
        ids.add(q.id);
      }
    }
    const ids = new Set();
    for (const m of data.mistakes) {
      question(m);
      if (ids.has(m.id) || !Number.isInteger(m.misses) || m.misses < 1 || !Number.isInteger(m.streak) || m.streak < 0 || m.streak > 2 || !validDate(m.lastSeen) || !submitted(m.lastAnswer)) fail();
      ids.add(m.id);
    }
    const stageStats = {};
    if (data.version === 2) {
      if (!data.stageStats || typeof data.stageStats !== 'object' || Array.isArray(data.stageStats)) fail();
      for (const [key, item] of Object.entries(data.stageStats)) {
        if (!Object.hasOwn(PRESETS, key) || !item || !['sessions', 'total', 'correct'].every(k => Number.isSafeInteger(item[k]) && item[k] >= 0) || item.correct > item.total || item.total > item.sessions * 50 || !Array.isArray(item.recentAccuracy) || item.recentAccuracy.length > 3 || item.recentAccuracy.some(x => typeof x !== 'number' || !Number.isFinite(x) || x < 0 || x > 1)) fail();
        stageStats[key] = { sessions: item.sessions, total: item.total, correct: item.correct, recentAccuracy: [...item.recentAccuracy] };
      }
    }
    // Only retain known fields; imported data cannot inject arbitrary object properties.
    return {
      version: 2, stageStats,
      sessions: data.sessions.map(s => ({ id: s.id, date: s.date, mode: s.mode, seconds: s.seconds, ...(s.stage ? { stage: s.stage } : {}), ...(s.timing ? { timing: s.timing } : {}), results: s.results.map(q => ({ ...canonicalQuestion(q), submitted: q.submitted, correct: q.correct })) })),
      mistakes: data.mistakes.map(m => ({ ...canonicalQuestion(m), misses: m.misses, streak: m.streak, lastSeen: m.lastSeen, lastAnswer: m.lastAnswer }))
    };
  }
  return { expression, explain: L.explain, parseAnswer, progress, PRESETS, SYMBOLS, blankState, validateConfig, makeQuestion, buildPool, generate, grade, record, reviewQuestions, createTimer, assess, validateState };
});
