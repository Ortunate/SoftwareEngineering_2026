(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MathLearning = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const OPS = ['+', '-', '*', '/'];
  const SYMBOLS = { '+': '+', '-': '−', '*': '×', '/': '÷' };
  const RANGES = [10, 20, 50, 100, 1000, 10000];
  const PRESETS = {
    starter: { label: '一年级 · 基础', description: '20以内加减，练习数的分合', max: 20, operations: ['+', '-'] },
    growing: { label: '一年级 · 提升', description: '100以内加减，练习进退位', max: 100, operations: ['+', '-'] },
    explorer: { label: '二年级 · 表内', description: '表内乘法与整除，巩固口诀', max: 100, operations: ['*', '/'] },
    mixed: { label: '二年级 · 两步', description: '两步混合运算，认识运算顺序', max: 100, operations: ['+', '-', '*', '/'], mode: 'mixed' },
    wide: { label: '三年级 · 多位数', description: '千以内加减、多位数乘除一位数', max: 1000, operations: ['+', '-', '*', '/'], facts: 'wide' },
    brackets: { label: '三年级 · 综合', description: '三步与括号，可配合草稿纸', max: 1000, operations: ['+', '-', '*', '/'], mode: 'brackets', facts: 'wide', steps: 3 },
    challenge: { label: '数域进阶', description: '万以内整数运算，按能力选练', max: 10000, operations: ['+', '-', '*', '/'], facts: 'wide' },
    negative: { label: '课外拓展 · 负数', description: '正负数与两步运算，家长指导选学', max: 20, operations: ['+', '-'], mode: 'mixed', negative: true }
  };
  for (const preset of Object.values(PRESETS)) Object.assign(preset, { carry: 'any', mode: preset.mode || 'single', facts: preset.facts || 'table', steps: preset.steps || 2, negative: preset.negative || false });
  function normalize(input) {
    if (!input || !RANGES.includes(input.max)) throw new Error('请选择有效的数值范围。');
    const c = { max: input.max, count: input.count ?? 30, operations: input.operations, carry: input.carry ?? 'any', mode: input.mode ?? 'single', facts: input.facts ?? 'table', steps: input.steps ?? 2, negative: input.negative ?? false };
    if (!Number.isInteger(c.count) || c.count < 5 || c.count > 50) throw new Error('题量应为5–50道整数。');
    if (!Array.isArray(c.operations) || !c.operations.length || new Set(c.operations).size !== c.operations.length || c.operations.some(op => !OPS.includes(op))) throw new Error('请至少选择一种有效运算。');
    if (!['any', 'without', 'with'].includes(c.carry) || !['single', 'mixed', 'brackets'].includes(c.mode) || !['table', 'wide'].includes(c.facts) || ![2, 3].includes(c.steps) || typeof c.negative !== 'boolean') throw new Error('练习条件不合法。');
    if ((c.mode !== 'single' || c.negative) && c.carry !== 'any') throw new Error('复合运算或负数模式请将进退位设置为不限。');
    return { ...c, operations: [...c.operations] };
  }
  function evaluate(tree, limit = 10000, allowNegative = true) {
    let nodes = 0;
    function visit(node, depth) {
      if (++nodes > 7 || depth > 3) throw new Error('算式超过三步限制。');
      if (typeof node === 'number') {
        if (!Number.isInteger(node) || Math.abs(node) > limit || (!allowNegative && node < 0)) throw new Error('数字超出范围。');
        return node;
      }
      if (!node || typeof node !== 'object' || Array.isArray(node) || !OPS.includes(node.op) || (node.group !== undefined && typeof node.group !== 'boolean')) throw new Error('算式结构无效。');
      const a = visit(node.left, depth + 1), b = visit(node.right, depth + 1);
      if (node.op === '/' && (b === 0 || a % b !== 0)) throw new Error('仅支持非零除数的整除。');
      const value = node.op === '+' ? a + b : node.op === '-' ? a - b : node.op === '*' ? a * b : a / b;
      if (!Number.isSafeInteger(value) || Math.abs(value) > limit || (!allowNegative && value < 0)) throw new Error('中间结果超出范围。');
      return value === 0 ? 0 : value;
    }
    return visit(tree, 0);
  }
  function cleanTree(node) {
    return typeof node === 'number' ? (node === 0 ? 0 : node) : { op: node.op, left: cleanTree(node.left), right: cleanTree(node.right), ...(node.group ? { group: true } : {}) };
  }
  function expression(tree) {
    if (typeof tree === 'number') return tree < 0 ? `(${tree})` : String(tree);
    const priority = op => ['*', '/'].includes(op) ? 2 : 1;
    function child(node, right) {
      const value = expression(node);
      if (typeof node === 'number' || node.group) return value;
      return priority(node.op) < priority(tree.op) || (right && priority(node.op) === priority(tree.op)) ? `(${value})` : value;
    }
    const text = `${child(tree.left, false)} ${SYMBOLS[tree.op]} ${child(tree.right, true)}`;
    return tree.group ? `(${text})` : text;
  }
  function operators(tree) { return typeof tree === 'number' ? [] : [...operators(tree.left), ...operators(tree.right), tree.op]; }
  function hasGroup(tree) { return typeof tree !== 'number' && (tree.group === true || hasGroup(tree.left) || hasGroup(tree.right)); }
  function hasNegative(tree) { return typeof tree === 'number' ? tree < 0 : evaluate(tree) < 0 || hasNegative(tree.left) || hasNegative(tree.right); }
  function hasCarry(a, b, op) {
    for (; a > 0 || b > 0; a = Math.floor(a / 10), b = Math.floor(b / 10)) {
      if (op === '+' ? a % 10 + b % 10 >= 10 : a % 10 < b % 10) return true;
    }
    return false;
  }
  function make(tree, input) {
    const profile = normalize(input);
    const answer = evaluate(tree, profile.max, profile.negative);
    const ops = operators(tree);
    if (!ops.length || ops.some(op => !profile.operations.includes(op)) || (profile.mode === 'single' ? ops.length !== 1 : ops.length !== profile.steps)) throw new Error('运算结构与配置不一致。');
    if ((profile.mode === 'brackets') !== Boolean(hasGroup(tree))) throw new Error('括号与题型不一致。');
    function checkFactors(node) {
      if (typeof node === 'number') return;
      const a = Math.abs(evaluate(node.left)), b = Math.abs(evaluate(node.right));
      if (node.op === '*' && (a < 1 || b < 1 || (profile.facts === 'table' ? a > 9 || b > 9 : b > 9))) throw new Error('乘数超出所选乘除范围。');
      if (node.op === '/' && (a < 1 || b < 1 || b > 9 || (profile.facts === 'table' && a / b > 9))) throw new Error('除法超出所选乘除范围。');
      checkFactors(node.left); checkFactors(node.right);
    }
    checkFactors(tree);
    if (profile.mode === 'single' && ['+', '-'].includes(tree.op) && profile.carry !== 'any' && hasCarry(tree.left, tree.right, tree.op) !== (profile.carry === 'with')) throw new Error('不符合进退位条件。');
    const signed = hasNegative(tree);
    const category = ops.length > 1 ? `${signed ? '负数·' : ''}${hasGroup(tree) ? '括号' : '混合'}${ops.length}步` : signed ? `负数${{ '+': '加法', '-': '减法', '*': '乘法', '/': '除法' }[tree.op]}` : tree.op === '+' || tree.op === '-' ? `${hasCarry(tree.left, tree.right, tree.op) ? (tree.op === '+' ? '进位' : '退位') : (tree.op === '+' ? '不进位' : '不退位')}${tree.op === '+' ? '加法' : '减法'}` : tree.op === '*' ? (Math.max(tree.left, tree.right) > 9 ? '多位数乘法' : '表内乘法') : Math.abs(answer) > 9 ? '多位数整除' : '整除除法';
    const canonical = cleanTree(tree);
    const { count, ...settings } = profile;
    return { kind: 'expression', id: `expr:${encodeURIComponent(JSON.stringify(canonical))}`, tree: canonical, profile: settings, answer, category };
  }
  function generate(input, random = Math.random) {
    const c = normalize(input);
    const integer = (low, high) => low + Math.floor(random() * (high - low + 1));
    const pick = values => values[integer(0, values.length - 1)];
    const sign = value => c.negative && random() < 0.5 ? -value : value;
    function single(op) {
      let a, b;
      if (op === '*') { b = integer(1, Math.min(9, c.max)); a = integer(1, c.facts === 'table' ? Math.min(9, Math.floor(c.max / b)) : Math.floor(c.max / b)); }
      else if (op === '/') { b = integer(1, Math.min(9, c.max)); const quotient = integer(1, c.facts === 'table' ? Math.min(9, Math.floor(c.max / b)) : Math.floor(c.max / b)); a = b * quotient; }
      else { a = integer(0, c.max); b = integer(0, c.max); }
      return { op, left: sign(a), right: sign(b) };
    }
    function compound() {
      const ops = Array.from({ length: c.steps }, () => pick(c.operations));
      // Modest operands keep compound exercises useful; all intermediates are checked.
      const ceiling = c.facts === 'table' ? Math.min(c.max, 30) : Math.min(c.max, 999);
      const values = Array.from({ length: c.steps + 1 }, () => sign(integer(1, ceiling)));
      for (let i = 0; i < ops.length; i++) if (['*', '/'].includes(ops[i])) values[i + 1] = sign(integer(1, 9));
      if (c.mode === 'brackets') {
        let tree = { op: ops[0], left: values[0], right: values[1], group: true };
        for (let i = 1; i < ops.length; i++) tree = { op: ops[i], left: tree, right: values[i + 1] };
        return tree;
      }
      // Fold multiplication/division first; then addition/subtraction, left-to-right.
      const terms = [values[0]], lowOps = [];
      for (let i = 0; i < ops.length; i++) {
        if (['*', '/'].includes(ops[i])) terms[terms.length - 1] = { op: ops[i], left: terms[terms.length - 1], right: values[i + 1] };
        else { lowOps.push(ops[i]); terms.push(values[i + 1]); }
      }
      let tree = terms[0];
      for (let i = 0; i < lowOps.length; i++) tree = { op: lowOps[i], left: tree, right: terms[i + 1] };
      return tree;
    }
    const result = [], ids = new Set();
    for (let attempt = 0; attempt < 30000 && result.length < c.count; attempt++) {
      try {
        const op = c.operations[result.length % c.operations.length];
        const q = make(c.mode === 'single' ? single(op) : compound(), c);
        if (c.negative && !hasNegative(q.tree)) continue;
        if (!ids.has(q.id)) { result.push(q); ids.add(q.id); }
      } catch { /* Reject invalid candidate; generation has a fixed attempt ceiling. */ }
    }
    if (result.length < c.count) throw new Error('当前条件难以组成足够的不重复题目，请减少题量、运算步数或放宽范围。');
    for (let i = result.length - 1; i > 0; i--) { const j = integer(0, i); [result[i], result[j]] = [result[j], result[i]]; }
    return result;
  }
  function explain(question) {
    if (question.kind !== 'expression') return [`${question.a} ${SYMBOLS[question.op]} ${question.b} = ${question.answer}`];
    const steps = [];
    function walk(node) {
      if (typeof node === 'number') return node;
      const left = walk(node.left), right = walk(node.right);
      const result = evaluate({ op: node.op, left, right });
      steps.push(`${expression({ op: node.op, left, right })} = ${result}`);
      return result;
    }
    walk(question.tree);
    return steps;
  }
  function referenceSeconds(q) {
    if (q.kind === 'expression') {
      const range = q.profile.max;
      const perStep = range > 1000 ? 45 : range > 100 ? 30 : q.profile.negative ? 20 : 18;
      return operators(q.tree).length * perStep;
    }
    if (q.op === '*') return 8;
    if (q.op === '/') return 10;
    const range = Math.max(q.a, q.b, q.answer);
    return range <= 10 ? 8 : range <= 20 ? 12 : 20;
  }
  return { PRESETS, normalize, evaluate, expression, operators, make, generate, explain, referenceSeconds };
});
