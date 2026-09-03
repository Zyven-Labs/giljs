'use strict';

/* Regression tests for the ten bug fixes shipped with this change.
 *
 * Each block corresponds to one bug and fails (throws / exits non-zero)
 * if the underlying defect regresses. Run standalone:
 *      node test/regression_test_bugfixes.js
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

console.log('=== regression: bug fixes ===');

/* Bug 1: parse_args double-free crash.
   exec(f, ['invalid', 123]) used to abort with a double free; it must now
   simply throw a JS TypeError. */
t('bug1: non-string arg throws instead of aborting/double-free', () => {
    const s = Script.load(`intent t() p <= not p end`);
    assert(throws(() => s.intent('t').execute(new Frontier(), ['invalid', 123])),
        'expected a thrown error');
});

/* Bug 2: Frontier::Set value validation.
   NaN folded to garbage and fractions silently truncated; both must now be
   rejected, while integral values still work. */
t('bug2: NaN value rejected', () => {
    const f = new Frontier();
    assert(throws(() => f.set('p', ['a'], NaN)), 'NaN must be rejected');
    assert.strictEqual(f.get('p', ['a']), GIL.FALSE);
});
t('bug2: fractional value rejected', () => {
    const f = new Frontier();
    assert(throws(() => f.set('p', ['a'], 1.9)), '1.9 must be rejected');
});
t('bug2: integral value accepted', () => {
    const f = new Frontier();
    f.set('p', ['a'], 1.0);
    assert.strictEqual(f.get('p', ['a']), GIL.TRUE);
});

/* Bug 3: query repeated-variable constraint.
   query(name, ['A','A']) must only match columns with equal values. */
t('bug3: query repeated variable only matches equal values', () => {
    const f = new Frontier();
    f.set('linked', ['x', 'x'], GIL.TRUE);
    f.set('linked', ['x', 'y'], GIL.TRUE);
    const r = f.query('linked', ['A', 'A']);
    assert.strictEqual(r.matches.length, 1, 'expected 1 match, got ' + r.matches.length);
    assert.deepStrictEqual(r.matches[0].args, ['x', 'x']);
});

/* Bug 4: repeat oscillation non-termination.
   `repeat q <= not q end` used to loop forever; it must now terminate. */
t('bug4: oscillating repeat terminates', () => {
    const s = Script.load(`intent t() repeat q <= not q end end`);
    const f = new Frontier();
    f.set('q', GIL.TRUE);
    s.intent('t').execute(f); // must not hang
});

/* Bug 5: parser constant-folding overflow.
   Huge constants used to wrap silently; now reported as an error. */
t('bug5: constant-folding overflow is an error', () => {
    assert(throws(() => Script.load(
        `intent t() score[alice, 4000000000 * 4000000000] <= true end`)),
        'fold overflow must be rejected');
});

/* Bug 6: runtime ADD/SUB overflow in exec.
   X + 1 / X - 1 with X at LONG_MAX / LONG_MIN used to wrap; now errors. */
t('bug6: runtime ADD overflow errors', () => {
    const s = Script.load(`intent t(X) r[X, X + 1] <= true end`);
    assert(throws(() => s.intent('t').execute(new Frontier(),
        ['9223372036854775807'])), 'ADD overflow must error');
});
t('bug6: runtime SUB overflow errors', () => {
    const s = Script.load(`intent t(X) r[X, X - 1] <= true end`);
    assert(throws(() => s.intent('t').execute(new Frontier(),
        ['-9223372036854775808'])), 'SUB overflow must error');
});
t('bug6: non-overflow arithmetic still works', () => {
    const s = Script.load(`intent t(X) r[X, X + 1] <= true end`);
    const f = new Frontier();
    s.intent('t').execute(f, ['5']);
    assert.strictEqual(f.get('r', ['5', '6']), GIL.TRUE);
});

/* Bug 7: frontier open-addressing deletion.
   Deleting predicates used to break later lookups of colliding entries. */
t('bug7: lookups survive many deletions', () => {
    const f = new Frontier();
    const N = 2000;
    for (let i = 0; i < N; i++) f.set('pred' + i, GIL.TRUE);
    for (let i = 0; i < N; i += 2) f.del('pred' + i);
    let lost = 0;
    for (let i = 1; i < N; i += 2) {
        if (f.get('pred' + i) !== GIL.TRUE) lost++;
    }
    assert.strictEqual(lost, 0, 'lost ' + lost + ' lookups after deletions');
});

/* Bug 8: intent-level parse error message preserved.
   A specific descendant error (division by zero, missing ')') used to be
   overwritten by a generic "expected 'end'". */
t('bug8: intent division-by-zero message preserved', () => {
    let msg = '';
    try { Script.load(`intent t() score[alice, 10 / 0] <= true end`); }
    catch (e) { msg = e.message; }
    assert(/division by zero/.test(msg), 'got: ' + msg);
});
t('bug8: intent missing-close-paren message preserved', () => {
    let msg = '';
    try { Script.load(`intent t() score[alice, (1] <= true end`); }
    catch (e) { msg = e.message; }
    assert(/expected '\)'/.test(msg), 'got: ' + msg);
});

/* Bug 9: Frontier::Set accepts boolean values.
   set('p', true) / set('p', ['a'], false) used to fail with a confusing
   "args must be a string" / "A number was expected" error. */
t('bug9: boolean value accepted', () => {
    const f = new Frontier();
    f.set('p', true);
    assert.strictEqual(f.get('p'), GIL.TRUE);
    f.set('p', ['a'], false);
    assert.strictEqual(f.get('p', ['a']), GIL.FALSE);
});

/* Bug 10: when/repeat-level parse error message preserved.
   Same error-clobbering issue one level deeper than Bug 8. */
t('bug10: when-body division-by-zero message preserved', () => {
    let msg = '';
    try { Script.load(`intent t() when p do q[alice, 10 / 0] <= true end end`); }
    catch (e) { msg = e.message; }
    assert(/division by zero/.test(msg), 'got: ' + msg);
});
t('bug10: repeat-body overflow message preserved', () => {
    let msg = '';
    try { Script.load(`intent t() repeat q[alice, 4000000000 * 4000000000] <= true end end`); }
    catch (e) { msg = e.message; }
    assert(/integer overflow/.test(msg), 'got: ' + msg);
});

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
