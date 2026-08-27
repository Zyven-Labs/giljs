'use strict';

const { Script, Frontier, GIL } = require('..');

let tests_run = 0;
let tests_ok  = 0;

function test(name, fn) {
    tests_run++;
    process.stdout.write(`  [${String(tests_run).padStart(3)}] ${name.padEnd(55)} `);
    try {
        fn();
        process.stdout.write('OK\n');
        tests_ok++;
    } catch (e) {
        process.stdout.write(`FAIL\n      ${e.message}\n`);
    }
}

function assert(cond, msg) {
    if (!cond) throw new Error(msg || 'assertion failed');
}

function assertVal(got, exp, label) {
    if (got !== exp) {
        throw new Error(`${label}: got ${got}, expected ${exp}`);
    }
}

/* ------------------------------------------------------------------ */
/* Frontier basics                                                    */
/* ------------------------------------------------------------------ */

function test_frontier_get_absent() {
    const f = new Frontier();
    assert(f !== null, 'frontier_new returned null');
    assertVal(f.get('alive'), GIL.FALSE, 'alive[]');
    assertVal(f.get('location', ['x']), GIL.FALSE, 'location[x]');
}

function test_frontier_set_get() {
    const f = new Frontier();
    f.set('alive', GIL.TRUE);
    assertVal(f.get('alive'), GIL.TRUE, 'alive');
}

function test_frontier_overwrite() {
    const f = new Frontier();
    f.set('location', ['alice', 'room1'], GIL.TRUE);
    assertVal(f.get('location', ['alice', 'room1']), GIL.TRUE, 'initial');
    f.set('location', ['alice', 'room1'], GIL.BOTH);
    assertVal(f.get('location', ['alice', 'room1']), GIL.BOTH, 'overwritten');
}

function test_frontier_del() {
    const f = new Frontier();
    f.set('owns', ['sword'], GIL.TRUE);
    assertVal(f.get('owns', ['sword']), GIL.TRUE, 'before del');
    f.del('owns', ['sword']);
    assertVal(f.get('owns', ['sword']), GIL.FALSE, 'after del');
}

function test_frontier_del_absent() {
    const f = new Frontier();
    f.del('nonexistent', ['x']);
}

function test_frontier_all_values() {
    const f = new Frontier();
    f.set('p1', GIL.FALSE);
    f.set('p2', GIL.TRUE);
    f.set('p3', GIL.BOTH);
    assertVal(f.get('p1'), GIL.FALSE, 'false');
    assertVal(f.get('p2'), GIL.TRUE, 'true');
    assertVal(f.get('p3'), GIL.BOTH, 'both');
}

function test_frontier_many_predicates() {
    const f = new Frontier();
    for (let i = 0; i < 200; i++) {
        f.set(`pred${i}`, [String(i)], GIL.TRUE);
    }
    for (let i = 0; i < 200; i++) {
        assertVal(f.get(`pred${i}`, [String(i)]), GIL.TRUE, `pred${i}`);
    }
}
/* ------------------------------------------------------------------ */
/* Script loading                                                     */
/* ------------------------------------------------------------------ */

function test_script_load_simple() {
    const src = `intent lightOn()
    lit <= true
end`;
    const s = Script.load(src);
    assert(s !== null, 'script loaded');
    const intent = s.intent('lightOn');
    assert(intent !== undefined, 'intent found');
}

function test_script_load_syntax_error() {
    try {
        Script.load('intent broken');
        throw new Error('expected error not thrown');
    } catch (e) {
        // expected
    }
}

function test_script_intent_not_found() {
    const src = `intent greet()
    greeting <= true
end`;
    const s = Script.load(src);
    const intent = s.intent('nonexistent');
    assert(intent === undefined, 'nonexistent intent');
}

/* ------------------------------------------------------------------ */
/* Intent execution                                                   */
/* ------------------------------------------------------------------ */

function test_intent_execute_simple() {
    const src = `intent lightOn()
    lit <= true
end`;
    const s = Script.load(src);
    const intent = s.intent('lightOn');
    const f = new Frontier();
    intent.execute(f);
    assertVal(f.get('lit'), GIL.TRUE, 'lit after lightOn');
}

function test_intent_execute_with_params() {
    const src = `intent setAt(Pred, Loc)
    Pred[Loc] <= true
end`;
    const s = Script.load(src);
    const intent = s.intent('setAt');
    const f = new Frontier();
    intent.execute(f, ['active', 'room1']);
    assertVal(f.get('active', ['room1']), GIL.TRUE, 'active[room1] after setAt');
}

function test_intent_propagate_active() {
    const src = `intent propagate_active(Node)
    activated[Node] <= true
    when activated[A] do
        when connected[A, B] do
            activated[B] <= true
        end
    end
end`;
    const s = Script.load(src);
    const intent = s.intent('propagate_active');
    const f = new Frontier();
    f.set('connected', ['a', 'b'], GIL.TRUE);
    f.set('connected', ['b', 'c'], GIL.TRUE);
    intent.execute(f, ['a']);
    assertVal(f.get('activated', ['a']), GIL.TRUE, 'activated[a]');
    assertVal(f.get('activated', ['b']), GIL.TRUE, 'activated[b]');
    assertVal(f.get('activated', ['c']), GIL.TRUE, 'activated[c]');
}

function test_intent_wrong_arg_count() {
    const src = `intent greet(Name)
    greeted[Name] <= true
end`;
    const s = Script.load(src);
    const intent = s.intent('greet');
    const f = new Frontier();
    try {
        intent.execute(f);  // missing required arg
        throw new Error('expected error not thrown');
    } catch (e) {
        // expected
    }
}

/* ------------------------------------------------------------------ */
/* Main test runner                                                   */
/* ------------------------------------------------------------------ */

const tests = [
    // Frontier
    test_frontier_get_absent,
    test_frontier_set_get,
    test_frontier_overwrite,
    test_frontier_del,
    test_frontier_del_absent,
    test_frontier_all_values,
    test_frontier_many_predicates,
    // Script
    test_script_load_simple,
    test_script_load_syntax_error,
    test_script_intent_not_found,
    // Intent
    test_intent_execute_simple,
    test_intent_execute_with_params,
    test_intent_propagate_active,
    test_intent_wrong_arg_count,
];

console.log('=== giljs test suite ===\n');

for (const fn of tests) {
    // Derive a name from the function name
    const name = fn.name.replace(/^test_/, '').replace(/_/g, ' ');
    test(name, fn);
}

console.log(`\nResults: ${tests_ok}/${tests_run} tests passed`);
process.exit(tests_ok === tests_run ? 0 : 1);
