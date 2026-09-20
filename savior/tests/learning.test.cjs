'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const C = require('../src/core.js');
const L = require('../src/learning.js');
function rng(seed) { return () => { seed = (1664525 * seed + 1013904223) >>> 0; return seed / 4294967296; }; }
const cfg = overrides => ({ max: 100, count: 30, operations: ['+', '-', '*', '/'], mode: 'mixed', facts: 'wide', ...overrides });
const node = (op, left, right, group) => ({ op, left, right, ...(group ? { group: true } : {}) });
function oracle(t) {
  if (typeof t === 'number') return t;
  const a = oracle(t.left), b = oracle(t.right);
  return { '+': () => a + b, '-': () => a - b, '*': () => a * b, '/': () => a / b }[t.op]();
}
function session(id, questions, stage) { return { id: String(id), date: '2026-09-20T10:00:00Z', mode: 'practice', seconds: 600, ...(stage ? { stage } : {}), results: C.grade(questions, Object.fromEntries(questions.map(q => [q.id, q.answer]))) }; }
test('T20: all eight stages generate 50 distinct, correct questions across 20 seeds', () => {
  for (const preset of Object.values(C.PRESETS)) for (let seed = 1; seed <= 20; seed++) {
    const questions = C.generate({ ...preset, count: 50 }, rng(seed));
    assert.equal(questions.length, 50);
    assert.equal(new Set(questions.map(q => q.id)).size, 50);
    for (const q of questions) {
      assert.ok(Math.abs(q.answer) <= preset.max);
      if (q.kind) {
        assert.equal(oracle(q.tree), q.answer);
        assert.equal(L.evaluate(q.tree, preset.max, preset.negative), q.answer);
        assert.equal(L.operators(q.tree).length, preset.mode === 'single' ? 1 : preset.steps);
        assert.ok(!/["<>]/.test(q.id));
        assert.deepEqual(L.make(q.tree, q.profile), q);
      }
    }
  }
});
test('T21: order, brackets, subtraction and division preserve exact meaning', () => {
  const a = L.make(node('+', 2, node('*', 3, 4)), cfg());
  const b = L.make(node('*', node('+', 2, 3, true), 4), cfg({ mode: 'brackets' }));
  assert.equal(a.answer, 14); assert.equal(C.expression(a), '2 + 3 × 4');
  assert.equal(b.answer, 20); assert.equal(C.expression(b), '(2 + 3) × 4');
  assert.deepEqual(C.explain(b), ['2 + 3 = 5', '5 × 4 = 20']);
  assert.equal(L.evaluate(node('/', node('/', 24, 4), 2)), 3);
  assert.equal(L.expression(node('-', 10, node('-', 5, 2))), '10 − (5 − 2)');
  assert.throws(() => L.evaluate(node('/', 4, 0)));
  assert.throws(() => L.evaluate(node('/', 7, 2)));
  assert.throws(() => L.evaluate(node('-', node('+', 90, 20), 50), 100));
  assert.throws(() => L.evaluate(node('+', -1, 2), 100, false));
});
test('T22: signed and fullwidth integer answers are accepted without partial parsing', () => {
  for (const text of ['-12', ' −12 ', '－１２']) assert.equal(C.parseAnswer(text), -12);
  assert.equal(C.parseAnswer('１００００'), 10000);
  for (const text of ['', '2.5', '1+2', '--1', '3oops', '100000']) assert.equal(C.parseAnswer(text), null);
  const q = L.make(node('-', 2, 9), cfg({ mode: 'single', negative: true }));
  assert.equal(C.grade([q], { [q.id]: '−7' })[0].correct, true);
});
test('T23: multi-digit carry filters check hundreds and borrowing across zeros', () => {
  const c = cfg({ max: 1000, mode: 'single', carry: 'with' });
  assert.equal(L.make(node('+', 150, 150), c).answer, 300);
  assert.equal(L.make(node('-', 400, 110), c).answer, 290);
  assert.throws(() => L.make(node('-', 400, 100), c));
});
test('T24: v1 migration, expression backup round-trip and tamper rejection', () => {
  assert.deepEqual(C.validateState({ version: 1, sessions: [], mistakes: [] }), C.blankState());
  const questions = C.generate({ ...C.PRESETS.negative, count: 10 }, rng(7));
  const s = session('new', questions, 'negative'); s.results[0] = { ...s.results[0], correct: false, submitted: null };
  const state = C.record(C.blankState(), s);
  assert.deepEqual(C.validateState(JSON.parse(JSON.stringify(state))), state);
  for (const mutate of [x => x.sessions[0].results[0].answer++, x => x.sessions[0].results[0].tree.op = 'eval', x => x.stageStats.negative.correct = Infinity, x => x.sessions[0].stage = 'bad']) {
    const bad = structuredClone(state); mutate(bad); assert.throws(() => C.validateState(bad));
  }
  const review = C.reviewQuestions(state, rng(8));
  assert.ok(review.some(q => q.id === questions[0].id));
  assert.ok(review.every(q => q.kind === 'expression' && q.profile.negative));
});
test('T25: stage aggregates survive detailed-history retention and only count qualifying sets', () => {
  const questions = C.generate({ ...C.PRESETS.starter, count: 10 }, rng(3));
  let state = C.blankState();
  for (let i = 0; i < 205; i++) state = C.record(state, session(i, questions, 'starter'));
  assert.equal(state.sessions.length, 200);
  assert.equal(C.progress(state, 'starter').total, 2050);
  assert.equal(C.progress(state, 'starter').ready, true);
  assert.deepEqual(C.validateState(state), state);
  const small = C.record(C.blankState(), session('small', questions.slice(0, 5), 'starter'));
  assert.equal(C.progress(small, 'starter').ready, false);
  const bad = session('bad', questions, 'starter'); bad.results = C.grade(questions, {});
  state = C.record(state, bad);
  assert.equal(C.progress(state, 'starter').ready, false);
});
test('T26: advanced timing scales by steps and generation has a bounded failure', () => {
  const q = L.make(node('*', node('+', 150, 120, true), 3), cfg({ max: 1000, mode: 'brackets' }));
  assert.equal(L.referenceSeconds(q), 60);
  assert.throws(() => L.generate(cfg(), () => 0), /减少题量/);
});

test('T27: progress distinguishes no records, short sets, partial samples and readiness', () => {
  const old = C.validateState({ version: 1, sessions: [], mistakes: [] });
  assert.equal(C.progress(old, 'starter').sampleCount, 0);
  assert.match(C.progress(old, 'starter').message, /暂无本阶段记录/);
  const questions = C.generate({ ...C.PRESETS.starter, count: 10 }, rng(11));
  let state = C.record(old, session('short', questions.slice(0, 5), 'starter'));
  assert.match(C.progress(state, 'starter').message, /每次都不足10题/);
  for (let n = 1; n <= 3; n++) {
    state = C.record(state, session(n, questions, 'starter'));
    const p = C.progress(state, 'starter');
    assert.equal(p.sampleCount, n);
    assert.equal(p.ready, n === 3);
    if (n < 3) {
      assert.ok(p.message.includes(`已有${n}次`));
      assert.ok(p.message.includes(`还需${3 - n}次`));
      assert.ok(!p.message.includes('最近3次'));
    }
  }
  const wrong = session('wrong', questions, 'starter'); wrong.results = C.grade(questions, {});
  state = C.record(state, wrong);
  assert.equal(C.progress(state, 'starter').ready, false);
  assert.match(C.progress(state, 'starter').message, /最近3次.*2次正确率/);
});
