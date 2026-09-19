'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const C = require('../src/core.js');
const config = (overrides = {}) => ({ max: 20, count: 30, carry: 'any', operations: ['+', '-'], ...overrides });
function rng(seed = 42) { return () => { seed = (1664525 * seed + 1013904223) >>> 0; return seed / 4294967296; }; }
function session(id, results) { return { id, date: '2026-09-19T10:00:00.000Z', mode: 'practice', seconds: 60, results }; }

test('T01: all ranges produce unique, bounded, nonnegative integer exercises', () => {
  for (const max of [10, 20, 50, 100]) {
    for (let seed = 1; seed <= 20; seed++) {
      const questions = C.generate(config({ max, count: 50, operations: ['+', '-', '*', '/'] }), rng(seed));
      assert.equal(questions.length, 50);
      assert.equal(new Set(questions.map(q => q.id)).size, 50);
      for (const q of questions) {
        assert.ok(q.a >= 0 && q.a <= max && q.b >= 0 && q.b <= max);
        assert.ok(Number.isInteger(q.answer) && q.answer >= 0 && q.answer <= max);
        if (q.op === '/') assert.ok(q.b >= 1 && q.b <= 9 && q.answer <= 9);
        if (q.op === '*') assert.ok(q.a >= 1 && q.a <= 9 && q.b >= 1 && q.b <= 9);
      }
    }
  }
});
test('T02: carry and borrow filters match arithmetic rules', () => {
  for (const carry of ['with', 'without']) {
    const pool = C.buildPool(config({ max: 100, carry }));
    assert.ok(pool.length);
    for (const q of pool) {
      const actual = q.op === '+' ? q.a % 10 + q.b % 10 >= 10 : q.a % 10 < q.b % 10;
      assert.equal(actual, carry === 'with');
    }
  }
});
test('T03: impossible exercise fails clearly rather than looping or duplicating', () => {
  assert.throws(() => C.generate(config({ max: 10, carry: 'with', operations: ['-'], count: 50 })), /减少题量/);
});
test('T04: invalid configuration is rejected', () => {
  for (const overrides of [{ count: 0 }, { count: 51 }, { count: 5.5 }, { max: 999 }, { operations: [] }, { operations: ['+','+'] }, { operations: ['%'] }, { carry: 'bad' }]) assert.throws(() => C.generate(config(overrides)));
});
test('T05: selected operations are balanced when all have capacity', () => {
  const questions = C.generate(config({ max: 100, count: 32, operations: ['+', '-', '*', '/'] }), rng());
  for (const op of ['+', '-', '*', '/']) assert.equal(questions.filter(q => q.op === op).length, 8);
});
test('T06: grade handles zero, whitespace, missing, invalid, and wrong answers', () => {
  const questions = [C.makeQuestion(3, 3, '-'), C.makeQuestion(2, 3, '+'), C.makeQuestion(3, 4, '*'), C.makeQuestion(8, 2, '/'), C.makeQuestion(9, 3, '/')];
  const results = C.grade(questions, { [questions[0].id]: '0', [questions[1].id]: ' 5 ', [questions[2].id]: '', [questions[3].id]: '4oops', [questions[4].id]: '2' });
  assert.deepEqual(results.map(q => q.correct), [true, true, false, false, false]);
  assert.equal(results[2].submitted, null);
  assert.equal(results[3].submitted, null);
});
test('T07: recording is immutable, idempotent and mastery requires two consecutive correct answers', () => {
  const q = C.makeQuestion(8, 7, '+');
  const original = C.blankState();
  let state = C.record(original, session('1', C.grade([q], {})));
  assert.equal(original.sessions.length, 0);
  assert.equal(state.mistakes[0].misses, 1);
  state = C.record(state, session('1', C.grade([q], {})));
  assert.equal(state.sessions.length, 1);
  assert.equal(state.mistakes[0].misses, 1);
  state = C.record(state, session('2', C.grade([q], { [q.id]: '15' })));
  assert.equal(state.mistakes[0].streak, 1);
  state = C.record(state, session('3', C.grade([q], { [q.id]: '15' })));
  assert.equal(state.mistakes[0].streak, 2);
  state = C.record(state, session('4', C.grade([q], { [q.id]: '14' })));
  assert.equal(state.mistakes[0].streak, 0);
  assert.equal(state.mistakes[0].misses, 2);
});
test('T08: review includes original plus unique same-category bounded variants', () => {
  const q = C.makeQuestion(8, 7, '+');
  const state = C.record(C.blankState(), session('1', C.grade([q], {})));
  const review = C.reviewQuestions(state, rng());
  assert.equal(review.length, 2);
  assert.ok(review.some(item => item.id === q.id));
  assert.equal(new Set(review.map(item => item.id)).size, 2);
  for (const item of review) { assert.equal(item.category, q.category); assert.ok(item.answer <= 20); }
  state.mistakes[0].streak = 2;
  assert.deepEqual(C.reviewQuestions(state), []);
});
test('T09: backup round trip keeps canonical data and drops unknown fields', () => {
  const q = C.makeQuestion(8, 7, '+');
  const state = C.record(C.blankState(), session('1', C.grade([q], {})));
  const source = JSON.parse(JSON.stringify(state));
  source.extra = '<script>bad</script>';
  assert.deepEqual(C.validateState(source), state);
});
test('T10: tampered backup is rejected before replacement', () => {
  const q = C.makeQuestion(8, 7, '+');
  const source = C.record(C.blankState(), session('1', C.grade([q], {})));
  const mutations = [s => { s.version = 5; }, s => { s.sessions[0].results[0].answer = 99; }, s => { s.sessions[0].results[0].correct = true; }, s => { s.mistakes[0].streak = 3; }, s => { s.sessions[0].date = 'bad'; }, s => { s.sessions.push(s.sessions[0]); }, s => { s.mistakes.push(s.mistakes[0]); }, s => { s.mistakes[0].op = 'evil'; }];
  for (const mutate of mutations) { const changed = JSON.parse(JSON.stringify(source)); mutate(changed); assert.throws(() => C.validateState(changed)); }
});
test('T11: history retention caps at 200 and removes oldest sessions', () => {
  const q = C.makeQuestion(1, 1, '+');
  let state = C.blankState();
  for (let i = 0; i < 205; i++) state = C.record(state, session(String(i), C.grade([q], { [q.id]: '2' })));
  assert.equal(state.sessions.length, 200);
  assert.equal(state.sessions[0].id, '5');
});
test('T12: grading and persistence support every operation', () => {
  const questions = C.generate(config({ max: 100, operations: ['+', '-', '*', '/'] }), rng());
  const results = C.grade(questions, Object.fromEntries(questions.map(q => [q.id, String(q.answer)])));
  assert.ok(results.every(q => q.correct));
  const state = C.record(C.blankState(), session('all', results));
  assert.equal(state.mistakes.length, 0);
  assert.deepEqual(C.validateState(state), state);
});
test('T14: clock accumulates only running periods and retains fractional seconds', () => {
  let now = 0;
  const clock = C.createTimer(() => now);
  assert.equal(clock.seconds(), 0);
  clock.setRunning(true);
  now = 1250;
  clock.setRunning(false);
  assert.equal(clock.seconds(), 1);
  now = 20000;
  assert.equal(clock.seconds(), 1);
  clock.setRunning(true);
  now = 20750;
  assert.equal(clock.seconds(), 2);
  clock.setRunning(false);
  now = 100000;
  assert.equal(clock.seconds(), 2);
  assert.equal(clock.isRunning(), false);
});
test('T15: repeated timer ticks do not double count elapsed time', () => {
  let now = 0;
  const clock = C.createTimer(() => now);
  clock.setRunning(true);
  for (let i = 1; i <= 30; i++) { now = i * 1000; clock.setRunning(true); }
  assert.equal(clock.seconds(), 30);
  clock.setRunning(false);
  now = 60000;
  clock.setRunning(false);
  assert.equal(clock.seconds(), 30);
});
function evaluatedResults(count = 10, wrong = 0, missing = 0) {
  return Array.from({ length: count }, (_, i) => ({ ...C.makeQuestion(1, 1, '+'), submitted: i < missing ? null : i < wrong + missing ? 3 : 2, correct: i >= wrong + missing }));
}
test('T16: performance keeps accuracy independent from speed and prioritizes errors', () => {
  const poor = evaluatedResults(10, 2);
  assert.equal(C.assess(poor, 10).tier, 'accuracy');
  assert.equal(C.assess(poor, 100).accuracy, 80);
  assert.equal(C.assess(poor, 10).accuracy, 80);
  assert.equal(C.assess(evaluatedResults(10, 0, 1), 10).tier, 'incomplete');
});
test('T17: performance handles the 90 percent and time thresholds without rounding errors', () => {
  assert.equal(C.assess(evaluatedResults(10, 1), 80).tier, 'fluent');
  assert.equal(C.assess(evaluatedResults(10, 1), 81).tier, 'steady');
  assert.equal(C.assess(evaluatedResults(10), 79).label, '正确又顺畅');
  assert.equal(C.assess(evaluatedResults(5), 4).tier, 'insufficient');
  assert.equal(C.assess(evaluatedResults(4), 32).tier, 'insufficient');
  assert.throws(() => C.assess([], 1));
});
test('T18: mixed-operation timing reference is calculated per question', () => {
  const questions = [C.makeQuestion(1, 1, '+'), C.makeQuestion(8, 7, '+'), C.makeQuestion(81, 9, '-'), C.makeQuestion(8, 7, '*'), C.makeQuestion(56, 7, '/')];
  const results = C.grade(questions, Object.fromEntries(questions.map(q => [q.id, String(q.answer)])));
  const assessment = C.assess(results, 58);
  assert.equal(assessment.referenceSeconds, (8 + 12 + 20 + 8 + 10) / 5);
  assert.equal(assessment.averageSeconds, 58 / 5);
  assert.equal(assessment.tier, 'fluent');
});
test('T19: old backups remain valid and new timer metadata survives round trip', () => {
  const q = C.makeQuestion(1, 1, '+');
  const s = session('timer', C.grade([q], { [q.id]: '2' }));
  for (const timing of [undefined, 'active-v1', 'paper-v1']) {
    const data = C.record(C.blankState(), { ...s, ...(timing ? { timing } : {}) });
    assert.deepEqual(C.validateState(data), data);
  }
  const invalid = C.record(C.blankState(), { ...s, timing: 'invalid' });
  assert.throws(() => C.validateState(invalid));
  assert.throws(() => C.generate(config({ operations: ['toString'] })));
});
