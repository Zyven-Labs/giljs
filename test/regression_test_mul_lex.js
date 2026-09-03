'use strict';

/* Regression tests for the three bug fixes shipped with this change:
 *
 *   1. parser.c arg_make_bin — MUL overflow guard used the wrong boundary
 *      for negative operands, so any literal product involving a negative
 *      (abs > 1) falsely reported "integer overflow".
 *   2. exec.c arg_eval — the same wrong boundary, applied at runtime.
 *   3. lexer.c — unknown characters were silently skipped instead of being
 *      reported, so malformed programs loaded successfully.
 *
 * Each block fails (throws / exits non-zero) if the underlying defect
 * regresses. Run standalone:
 *      node test/regression_test_mul_lex.js
 */

const assert = require('assert');
const { Script, Frontier, GIL } = require('..');

let pass = 0;
let fail = 0;

function t(name, fn) {
    try {
        fn();
        console.log('  PASS', name);
        pass++;
    } catch (e) {
        console.error('  FAIL', name, '->', e.message);
        fail++;
    }
}

function throws(fn) {
    let threw = false;
    try { fn(); } catch (e) { threw = true; }
    return threw;
}

console.log('=== regression: MUL overflow guard + lexer invalid character ===');

/* Bug 1 (parse-time): negative-operand MUL no longer falsely overflows. */
t('mul parse: negative*negative product computes', () => {
    const s = Script.load('intent t() p[(1-5)*(1-5)] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f);
    assert.strictEqual(f.get('p', ['16']), GIL.TRUE, 'expected p[16] true');
});
t('mul parse: negative*positive product computes', () => {
    const s = Script.load('intent t() p[(1-5)*2] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f);
    assert.strictEqual(f.get('p', ['-8']), GIL.TRUE, 'expected p[-8] true');
});
t('mul parse: positive*negative product computes', () => {
    const s = Script.load('intent t() p[2*(1-5)] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f);
    assert.strictEqual(f.get('p', ['-8']), GIL.TRUE, 'expected p[-8] true');
});
t('mul parse: small negative literal product computes', () => {
    // (1-5)*(1-5)=16 and 2*(1-5)=-8 previously tripped the wrong boundary.
    const s = Script.load('intent t() p[(1-5)*(1-5)] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f);
    assert.strictEqual(f.get('p', ['16']), GIL.TRUE, 'expected p[16] true');
    const s2 = Script.load('intent t() q[2*(1-5)] <= true end');
    const f2 = new Frontier();
    s2.intent('t').execute(f2);
    assert.strictEqual(f2.get('q', ['-8']), GIL.TRUE, 'expected q[-8] true');
});

/* Bug 1 (parse-time): genuine overflow is still rejected. */
t('mul parse: genuine overflow is still an error', () => {
    assert(throws(() => Script.load(
        'intent t() p[4000000000*4000000000] <= true end')),
        'genuine overflow must be rejected');
});

/* Bug 2 (runtime): negative-operand MUL no longer falsely overflows. */
t('mul runtime: negative*negative product computes', () => {
    const s = Script.load('intent t(X) r[X, X*X] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f, ['-4']);
    assert.strictEqual(f.get('r', ['-4', '16']), GIL.TRUE, 'expected r[-4,16] true');
});
t('mul runtime: negative*positive product computes', () => {
    const s = Script.load('intent t(X) r[X, X*2] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f, ['-4']);
    assert.strictEqual(f.get('r', ['-4', '-8']), GIL.TRUE, 'expected r[-4,-8] true');
});
t('mul runtime: positive*negative product computes', () => {
    const s = Script.load('intent t(X) r[X, X*(0-2)] <= true end');
    const f = new Frontier();
    s.intent('t').execute(f, ['4']);
    assert.strictEqual(f.get('r', ['4', '-8']), GIL.TRUE, 'expected r[4,-8] true');
});

/* Bug 2 (runtime): genuine overflow is still rejected. */
t('mul runtime: genuine overflow is still an error', () => {
    const s = Script.load('intent t(X) r[X, X*X] <= true end');
    assert(throws(() => s.intent('t').execute(new Frontier(),
        ['4000000000'])), 'genuine overflow must error');
});

/* Bug 3 (lexer): unknown characters now produce a load error. */
t('lex: unknown character inside an identifier is rejected', () => {
    assert(throws(() => Script.load('intent t() p <= tru@e end')),
        'unknown character must be rejected');
});
t('lex: unknown character standalone is rejected', () => {
    assert(throws(() => Script.load('intent t() p@ <= true end')),
        'unknown character must be rejected');
});
t('lex: unknown character in an argument is rejected', () => {
    assert(throws(() => Script.load('intent t() p[a$] <= true end')),
        'unknown character must be rejected');
});

/* Sanity: a valid program still loads and executes correctly. */
t('sanity: valid program still works', () => {
    const s = Script.load('intent t() p <= true end');
    const f = new Frontier();
    s.intent('t').execute(f);
    assert.strictEqual(f.get('p'), GIL.TRUE, 'expected p true');
});

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
