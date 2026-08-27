/* test.c -- comprehensive libgil test suite */

#include "gil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_ok  = 0;

#define TEST(name) do {                                         \
    tests_run++;                                                \
    printf("  [%3d] %-55s ", tests_run, name);                  \
    fflush(stdout);                                             \
} while(0)

#define OK() do { printf("OK\n"); tests_ok++; } while(0)

#define FAIL(msg) do {                                          \
    printf("FAIL\n      %s:%d: %s\n", __FILE__, __LINE__, msg);   \
    return;                                                     \
} while(0)

#define CHECK(cond, msg) do {                                   \
    if (!(cond)) FAIL(msg);                                     \
} while(0)

#define CHECK_VAL(got, exp, label) do {                         \
    if ((got) != (exp)) {                                       \
        printf("FAIL\n      %s: got %d, expected %d\n",         \
               label, (int)(got), (int)(exp));                  \
        return;                                                 \
    }                                                           \
} while(0)

/* ------------------------------------------------------------------ */
/* Section: Frontier basics                                           */
/* ------------------------------------------------------------------ */

static void test_frontier_get_absent(void)
{
    const char *a[] = {"x"};
    GilFrontier *f;
    GilVal v;
    TEST("frontier: get absent -> GIL_FALSE");
    f = gil_frontier_new(NULL);
    CHECK(f != NULL, "frontier_new returned NULL");
    v = gil_frontier_get(f, "alive", NULL, 0);
    CHECK_VAL(v, GIL_FALSE, "alive[]");
    v = gil_frontier_get(f, "location", a, 1);
    CHECK_VAL(v, GIL_FALSE, "location[x]");
    gil_frontier_free(f);
    OK();
}

static void test_frontier_set_get(void)
{
    GilFrontier *f;
    GilVal v;
    TEST("frontier: set then get round-trips");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "alive", NULL, 0, GIL_TRUE);
    v = gil_frontier_get(f, "alive", NULL, 0);
    CHECK_VAL(v, GIL_TRUE, "alive");
    gil_frontier_free(f);
    OK();
}

static void test_frontier_overwrite(void)
{
    GilFrontier *f;
    const char *a[] = {"alice", "room1"};
    GilVal v;
    TEST("frontier: overwrite changes value");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "location", a, 2, GIL_TRUE);
    v = gil_frontier_get(f, "location", a, 2);
    CHECK_VAL(v, GIL_TRUE, "initial");
    gil_frontier_set(f, "location", a, 2, GIL_BOTH);
    v = gil_frontier_get(f, "location", a, 2);
    CHECK_VAL(v, GIL_BOTH, "overwritten");
    gil_frontier_free(f);
    OK();
}

static void test_frontier_del(void)
{
    GilFrontier *f;
    const char *a[] = {"sword"};
    GilVal v;
    TEST("frontier: del removes predicate");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "owns", a, 1, GIL_TRUE);
    v = gil_frontier_get(f, "owns", a, 1);
    CHECK_VAL(v, GIL_TRUE, "before del");
    gil_frontier_del(f, "owns", a, 1);
    v = gil_frontier_get(f, "owns", a, 1);
    CHECK_VAL(v, GIL_FALSE, "after del");
    gil_frontier_free(f);
    OK();
}

static void test_frontier_del_absent(void)
{
    GilFrontier *f;
    TEST("frontier: del absent predicate is no-op");
    f = gil_frontier_new(NULL);
    gil_frontier_del(f, "nonexistent", NULL, 0);
    gil_frontier_free(f);
    OK();
}

static void test_frontier_all_values(void)
{
    GilFrontier *f;
    GilVal v;
    TEST("frontier: all three values set/get correctly");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "p", NULL, 0, GIL_TRUE);
    v = gil_frontier_get(f, "p", NULL, 0);
    CHECK_VAL(v, GIL_TRUE, "true");
    gil_frontier_set(f, "p", NULL, 0, GIL_FALSE);
    v = gil_frontier_get(f, "p", NULL, 0);
    CHECK_VAL(v, GIL_FALSE, "false");
    gil_frontier_set(f, "p", NULL, 0, GIL_BOTH);
    v = gil_frontier_get(f, "p", NULL, 0);
    CHECK_VAL(v, GIL_BOTH, "both");
    gil_frontier_free(f);
    OK();
}

static void test_frontier_many_predicates(void)
{
    GilFrontier *f;
    int i;
    char name[32];
    const char *a[1];
    GilVal v;
    TEST("frontier: 100 predicates all round-trip");
    f = gil_frontier_new(NULL);
    for (i = 0; i < 100; i++) {
        sprintf(name, "pred%d", i);
        a[0] = name;
        gil_frontier_set(f, "p", a, 1, (GilVal)(i % 3));
    }
    for (i = 0; i < 100; i++) {
        sprintf(name, "pred%d", i);
        a[0] = name;
        v = gil_frontier_get(f, "p", a, 1);
        CHECK_VAL(v, (GilVal)(i % 3), "round-trip");
    }
    gil_frontier_free(f);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Query                                                     */
/* ------------------------------------------------------------------ */

static void test_query_exact_match(void)
{
    GilFrontier *f;
    GilResult r;
    const char *a[] = {"alice", "sword"};
    TEST("query: exact literal match returns one result");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "owns", a, 2, GIL_TRUE);
    {
        const char *p[] = {"alice", "sword"};
        r = gil_frontier_query(f, "owns", p, 2);
    }
    if (r.count != 1) { printf("FAIL\n      count: got %lu, expected 1\n", (unsigned long)r.count); return; }
    CHECK_VAL(r.matches[0].value, GIL_TRUE, "value");
    CHECK(strcmp(r.matches[0].args[0], "alice") == 0, "args[0] mismatch");
    CHECK(strcmp(r.matches[0].args[1], "sword") == 0, "args[1] mismatch");
    gil_result_free(&r);
    gil_frontier_free(f);
    OK();
}

static void test_query_variable_match(void)
{
    GilFrontier *f;
    GilResult r;
    const char *a1[] = {"alice", "sword"};
    const char *a2[] = {"alice", "shield"};
    TEST("query: variable (uppercase) matches any concrete value");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "owns", a1, 2, GIL_TRUE);
    gil_frontier_set(f, "owns", a2, 2, GIL_TRUE);
    {
        const char *p[] = {"alice", "Item"};  /* 'Item' = variable */
        r = gil_frontier_query(f, "owns", p, 2);
    }
    if (r.count != 2) { printf("FAIL\n      count: got %lu, expected 2\n", (unsigned long)r.count); return; }
    CHECK(strcmp(r.matches[0].name, "owns") == 0, "name mismatch");
    gil_result_free(&r);
    gil_frontier_free(f);
    OK();
}

static void test_query_no_match(void)
{
    GilFrontier *f;
    GilResult r;
    const char *a[] = {"alice"};
    TEST("query: no match returns empty result");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "owns", a, 1, GIL_TRUE);
    {
        const char *p[] = {"bob"};
        r = gil_frontier_query(f, "owns", p, 1);
    }
    if (r.count != 0) { printf("FAIL\n      count: got %lu, expected 0\n", (unsigned long)r.count); return; }
    CHECK(r.matches != NULL, "matches should not be NULL");
    gil_result_free(&r);
    gil_frontier_free(f);
    OK();
}

static void test_query_empty_frontier(void)
{
    GilFrontier *f;
    GilResult r;
    TEST("query: empty frontier returns empty result");
    f = gil_frontier_new(NULL);
    {
        const char *p[] = {"X"};
        r = gil_frontier_query(f, "anything", p, 1);
    }
    if (r.count != 0) { printf("FAIL\n      count: got %lu, expected 0\n", (unsigned long)r.count); return; }
    gil_result_free(&r);
    gil_frontier_free(f);
    OK();
}

static void test_query_arg_count_mismatch(void)
{
    GilFrontier *f;
    GilResult r;
    const char *a[] = {"alice", "sword"};
    TEST("query: different arg count yields no match");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "owns", a, 2, GIL_TRUE);
    {
        const char *p[] = {"alice"};
        r = gil_frontier_query(f, "owns", p, 1);
    }
    if (r.count != 0) { printf("FAIL\n      count: got %lu, expected 0\n", (unsigned long)r.count); return; }
    gil_result_free(&r);
    gil_frontier_free(f);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Script loading                                            */
/* ------------------------------------------------------------------ */

static void test_load_empty(void)
{
    GilScript *s;
    const char *err = NULL;
    TEST("load: empty string yields valid (empty) script");
    s = gil_load("", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    CHECK(gil_intent_get(s, "anything") == NULL, "found intent in empty script");
    gil_script_free(s);
    OK();
}

static void test_load_whitespace_only(void)
{
    GilScript *s;
    const char *err = NULL;
    TEST("load: whitespace-only string yields valid script");
    s = gil_load("  \n  \n  ", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    gil_script_free(s);
    OK();
}

static void test_load_comments_only(void)
{
    GilScript *s;
    const char *err = NULL;
    TEST("load: comment-only string yields valid script");
    s = gil_load("# comment line\n# another", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    gil_script_free(s);
    OK();
}

static void test_load_single_intent(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: single simple intent parses");
    s = gil_load("intent hello() end\n", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    CHECK(gil_intent_get(s, "hello") != NULL, "intent 'hello' not found");
    gil_script_free(s);
    OK();
}

static void test_load_intent_with_params(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: intent with parameters parses");
    s = gil_load("intent move(Player, Dest) end\n", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    CHECK(gil_intent_get(s, "move") != NULL, "intent 'move' not found");
    gil_script_free(s);
    OK();
}

static void test_load_multiple_intents(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: multiple intents parse correctly");
    s = gil_load("intent a() end\nintent b() end\nintent c() end\n", &err);
    if (s == NULL) { printf("FAIL\n      gil_load returned NULL: %s\n", err ? err : "(no error)"); return; }
    CHECK(gil_intent_get(s, "a") != NULL, "intent 'a' not found");
    CHECK(gil_intent_get(s, "b") != NULL, "intent 'b' not found");
    CHECK(gil_intent_get(s, "c") != NULL, "intent 'c' not found");
    gil_script_free(s);
    OK();
}

static void test_load_syntax_error(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: garbage input reports error");
    s = gil_load("not a valid gil program !!!", &err);
    if (s != NULL) { printf("FAIL\n      expected NULL, got %p\n", (void*)s); return; }
    CHECK(err != NULL, "error string should not be NULL");
    if (s) gil_script_free(s);
    OK();
}

static void test_load_missing_end(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: missing 'end' reports parse error");
    s = gil_load("intent broken()\n", &err);
    CHECK(s == NULL, "expected NULL for missing end");
    CHECK(err != NULL, "error string should not be NULL");
    if (s) gil_script_free(s);
    OK();
}

static void test_intent_get_nonexistent(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: gil_intent_get non-existent returns NULL");
    s = gil_load("intent real() end\n", &err);
    if (s == NULL) { printf("FAIL\n      load failed: %s\n", err ? err : "?"); return; }
    if (s) {
        GilIntent *in = gil_intent_get(s, "fake");
        CHECK(in == NULL, "found non-existent intent");
        gil_script_free(s);
    }
    OK();
}

static void test_script_free_null(void)
{
    TEST("load: gil_script_free(NULL) does not crash");
    gil_script_free(NULL);
    OK();
}

static void test_frontier_free_null(void)
{
    TEST("frontier: gil_frontier_free(NULL) does not crash");
    gil_frontier_free(NULL);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Expression evaluation (via simple intents)                */
/* ------------------------------------------------------------------ */

static void test_exec_true_literal(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: assignment from literal true");
    s = gil_load("intent t()  p <= true  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_false_literal(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: assignment from literal false (no-op on frontier)");
    s = gil_load("intent t()  p <= false  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "p", NULL, 0, GIL_TRUE);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    /* false assignment should overwrite true (it's stored) */
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_both_literal(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: assignment from literal both");
    s = gil_load("intent t()  p <= both  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_BOTH, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_not_true(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: not true -> false");
    s = gil_load("intent t()  p <= not true  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "not true = false");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_not_false(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: not false -> true");
    s = gil_load("intent t()  p <= not false  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "not false = true");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_not_both(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: not both -> both");
    s = gil_load("intent t()  p <= not both  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_BOTH, "not both = both");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_and_truth_table(void)
{
    /* Test and with literal values as inputs */
    static const char *cases[][3] = {
        /* lhs, rhs, expected */
        {"true",  "true",  "true"},
        {"true",  "false", "false"},
        {"true",  "both",  "both"},
        {"false", "true",  "false"},
        {"false", "false", "false"},
        {"false", "both",  "false"},
        {"both",  "true",  "both"},
        {"both",  "false", "false"},
        {"both",  "both",  "both"},
    };
    int i;
    for (i = 0; i < 9; i++) {
        GilScript *s;
        GilIntent *in;
        GilFrontier *f;
        int rc;
        char buf[256];
        const char *lhs = cases[i][0];
        const char *rhs = cases[i][1];
        const char *exp = cases[i][2];
        GilVal expected;
        if (strcmp(exp, "true") == 0) expected = GIL_TRUE;
        else if (strcmp(exp, "both") == 0) expected = GIL_BOTH;
        else expected = GIL_FALSE;

        sprintf(buf, "exec: %s and %s -> %s", lhs, rhs, exp);
        TEST(buf);
        sprintf(buf, "intent t()  p <= %s and %s  end\n", lhs, rhs);
        s = gil_load(buf, NULL);
        if (!s) { printf("FAIL (skipping)\n"); continue; }
        in = gil_intent_get(s, "t");
        if (!in) { printf("FAIL (skipping)\n"); gil_script_free(s); continue; }
        f = gil_frontier_new(NULL);
        rc = gil_intent_execute(in, f, NULL, 0);
        if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
        CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), expected, "result");
        gil_frontier_free(f);
        gil_script_free(s);
        OK();
    }
}

static void test_exec_or_truth_table(void)
{
    static const char *cases[][3] = {
        {"true",  "true",  "true"},
        {"true",  "false", "true"},
        {"true",  "both",  "true"},
        {"false", "true",  "true"},
        {"false", "false", "false"},
        {"false", "both",  "both"},
        {"both",  "true",  "true"},
        {"both",  "false", "both"},
        {"both",  "both",  "both"},
    };
    int i;
    for (i = 0; i < 9; i++) {
        GilScript *s;
        GilIntent *in;
        GilFrontier *f;
        int rc;
        char buf[256];
        const char *lhs = cases[i][0];
        const char *rhs = cases[i][1];
        const char *exp = cases[i][2];
        GilVal expected;
        if (strcmp(exp, "true") == 0) expected = GIL_TRUE;
        else if (strcmp(exp, "both") == 0) expected = GIL_BOTH;
        else expected = GIL_FALSE;

        sprintf(buf, "exec: %s or %s -> %s", lhs, rhs, exp);
        TEST(buf);
        sprintf(buf, "intent t()  p <= %s or %s  end\n", lhs, rhs);
        s = gil_load(buf, NULL);
        if (!s) { printf("FAIL (skipping)\n"); continue; }
        in = gil_intent_get(s, "t");
        if (!in) { printf("FAIL (skipping)\n"); gil_script_free(s); continue; }
        f = gil_frontier_new(NULL);
        rc = gil_intent_execute(in, f, NULL, 0);
        if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
        CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), expected, "result");
        gil_frontier_free(f);
        gil_script_free(s);
        OK();
    }
}

static void test_exec_precedence(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: operator precedence: not > and > or");
    /* not false and true or false
       = (not false) and true or false
       = true and true or false
       = true or false
       = true */
    s = gil_load("intent t()  p <= not false and true or false  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "result");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_parens_override(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: parentheses override precedence");
    /* not (false and true) or false
       = not (false) or false
       = true or false = true */
    s = gil_load("intent t()  p <= not (false and true) or false  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "result");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_predicate_lookup(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: assignment from frontier predicate lookup");
    s = gil_load("intent t()  p <= has_key  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "has_key", NULL, 0, GIL_TRUE);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p = has_key");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_predicate_absent_is_false(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: absent predicate evaluates to false");
    s = gil_load("intent t()  p <= missing  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    /* missing is absent, so it's false. p <= false. */
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Parameterized intents                                     */
/* ------------------------------------------------------------------ */

static void test_exec_param_assignment(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    const char *args[] = {"alice"};
    const char *a[] = {"alice"};
    TEST("exec: parameterized assignment writes correct predicate");
    s = gil_load("intent set(P)  alive[P] <= true  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "set");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, args, 1);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "alive", a, 1), GIL_TRUE, "alive[alice]");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}
static void test_exec_param_as_predicate_name(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    const char *args[] = {"alive", "alice"};
    const char *a[] = {"alice"};
    TEST("exec: parameter as predicate name resolves correctly");
    s = gil_load("intent setAt(Pred, Loc)  Pred[Loc] <= true  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "setAt");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, args, 2);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "alive", a, 1), GIL_TRUE, "alive[alice]");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_exec_wrong_arg_count(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    const char *args[] = {"too", "many"};
    TEST("exec: wrong argument count returns -1");
    s = gil_load("intent one(P)  p <= true  end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "one");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, args, 2);
    if (rc != -1) { printf("FAIL\n      expected -1 for wrong argc, got %d\n", rc); return; }
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: When-blocks (simple guards)                               */
/* ------------------------------------------------------------------ */

static void test_when_guard_true(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: when true guard executes body");
    s = gil_load("intent t() when true do p <= true end end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_when_guard_false(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: when false guard skips body");
    s = gil_load("intent t() when false do p <= true end end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "p should stay false");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_when_predicate_guard(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: when predicate guard respects frontier");
    s = gil_load("intent t() when flag do p <= true end end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "t");
    CHECK(in != NULL, "intent not found");

    /* Case 1: flag absent => body skipped */
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "p with flag absent");
    gil_frontier_free(f);

    /* Case 2: flag = true => body executed */
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "flag", NULL, 0, GIL_TRUE);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p with flag true");
    gil_frontier_free(f);

    /* Case 3: flag = both => body executed */
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "flag", NULL, 0, GIL_BOTH);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p with flag both");
    gil_frontier_free(f);

    /* Case 4: flag = false => body skipped */
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "flag", NULL, 0, GIL_FALSE);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_FALSE, "p with flag false");
    gil_frontier_free(f);

    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: propagate_active (spec section 13)                        */
/* ------------------------------------------------------------------ */

static void test_propagate_active(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    const char *a1[] = {"start"};
    const char *a2[] = {"start", "middle"};
    const char *a3[] = {"middle", "end"};
    const char *ex1[] = {"start"};
    const char *ex2[] = {"middle"};
    const char *ex3[] = {"end"};

    TEST("exec: propagate_active converges correctly (spec \24713)");

    s = gil_load(
        "intent propagate_active(Node)\n"
        "\n"
        "    activated[Node] <= true\n"
        "\n"
        "    when activated[A] do\n"
        "        when connected[A, B] do\n"
        "            activated[B] <= true\n"
        "        end\n"
        "    end\n"
        "\n"
        "end\n",
        NULL);
    CHECK(s != NULL, "load propagate_active failed");

    in = gil_intent_get(s, "propagate_active");
    CHECK(in != NULL, "intent not found");

    f = gil_frontier_new(NULL);
    gil_frontier_set(f, "activated", a1, 1, GIL_TRUE);
    gil_frontier_set(f, "connected", a2, 2, GIL_TRUE);
    gil_frontier_set(f, "connected", a3, 2, GIL_TRUE);

    rc = gil_intent_execute(in, f, ex1, 1);
    if (rc != 0) { printf("FAIL\n      exec propagate_active returned %d\n", rc); return; }

    /* All three should now be true (converged). */
    CHECK_VAL(gil_frontier_get(f, "activated", ex1, 1), GIL_TRUE,
              "activated[start]");
    CHECK_VAL(gil_frontier_get(f, "activated", ex2, 1), GIL_TRUE,
              "activated[middle]");
    CHECK_VAL(gil_frontier_get(f, "activated", ex3, 1), GIL_TRUE,
              "activated[end]");

    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Convergence (stops when stable)                           */
/* ------------------------------------------------------------------ */

static void test_convergence_no_change(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: convergence stops when no changes (no infinite loop)");
    /* p <= true, then when p do p <= true end.
       First iter: p becomes true. Second iter: p already true, no change. */
    s = gil_load(
        "intent fixpoint()\n"
        "    p <= true\n"
        "    when p do\n"
        "        p <= true\n"
        "    end\n"
        "end\n",
        NULL);
    CHECK(s != NULL, "load fixpoint failed");
    in = gil_intent_get(s, "fixpoint");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec returned %d (expected 0 = convergence)\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p should be true");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Assignment resolution (true+false -> both)                */
/* ------------------------------------------------------------------ */

static void test_resolution_true_false(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: true+false resolves to both");
    s = gil_load(
        "intent conflict()\n"
        "    p <= true\n"
        "    p <= false\n"
        "end\n",
        NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "conflict");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_BOTH, "true+false = both");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_resolution_both_everything(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: true+both resolves to both");
    s = gil_load("intent c() p <= true  p <= both end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "c");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_BOTH, "true+both = both");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

/* ------------------------------------------------------------------ */
/* Section: Edge cases                                                */
/* ------------------------------------------------------------------ */

static void test_nested_when(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    TEST("exec: triply nested when blocks");
    s = gil_load(
        "intent n()\n"
        "    when true do\n"
        "        when true do\n"
        "            when true do\n"
        "                p <= true\n"
        "            end\n"
        "        end\n"
        "    end\n"
        "end\n",
        NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "n");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, NULL, 0);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_single_param_zero_arg(void)
{
    GilScript *s;
    GilIntent *in;
    GilFrontier *f;
    int rc;
    const char *args[] = {"x"};
    TEST("exec: intent with param, called with one arg");
    s = gil_load("intent id(P) p <= true end\n", NULL);
    CHECK(s != NULL, "load failed");
    in = gil_intent_get(s, "id");
    CHECK(in != NULL, "intent not found");
    f = gil_frontier_new(NULL);
    rc = gil_intent_execute(in, f, args, 1);
    if (rc != 0) { printf("FAIL\n      exec rc=%d\n", rc); return; }
    CHECK_VAL(gil_frontier_get(f, "p", NULL, 0), GIL_TRUE, "p");
    gil_frontier_free(f);
    gil_script_free(s);
    OK();
}

static void test_frontier_set_null_frontier(void)
{
    TEST("frontier: set() with NULL frontier does not crash");
    gil_frontier_set(NULL, "x", NULL, 0, GIL_TRUE);
    OK();
}

static void test_frontier_get_null_frontier(void)
{
    GilVal v;
    TEST("frontier: get() with NULL frontier returns GIL_FALSE");
    v = gil_frontier_get(NULL, "x", NULL, 0);
    CHECK_VAL(v, GIL_FALSE, "result");
    OK();
}

static void test_frontier_set_null_name(void)
{
    GilFrontier *f;
    TEST("frontier: set() with NULL name does not crash");
    f = gil_frontier_new(NULL);
    gil_frontier_set(f, NULL, NULL, 0, GIL_TRUE);
    gil_frontier_free(f);
    OK();
}
static void test_frontier_set_temporary_name(void)
{
    GilFrontier *f;
    char *name;
    GilVal v;
    TEST("frontier: set with freed name survives (deep copy)");
    f = gil_frontier_new(NULL);
    CHECK(f != NULL, "frontier_new returned NULL");
    name = (char*)malloc(6);
    CHECK(name != NULL, "malloc failed");
    strcpy(name, "temp");
    gil_frontier_set(f, name, NULL, 0, GIL_TRUE);
    free(name);
    v = gil_frontier_get(f, "temp", NULL, 0);
    CHECK_VAL(v, GIL_TRUE, "temp after name freed");
    gil_frontier_free(f);
    OK();
}

static void test_result_free_null(void)
{
    GilResult r;
    TEST("query: gil_result_free(NULL) does not crash");
    r.matches = NULL; r.count = 0;
    gil_result_free(&r);
    OK();
}

static void test_load_file_nonexistent(void)
{
    const char *err = NULL;
    GilScript *s;
    TEST("load: gil_load_file nonexistent returns NULL");
    s = gil_load_file("/tmp/__nonexistent_gil_file_xyzzy__.gil", &err);
    CHECK(s == NULL, "expected NULL");
    CHECK(err != NULL, "error should not be NULL");
    OK();
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== libgil test suite ===\n\n");

    printf("--- Frontier basics ---\n");
    test_frontier_get_absent();
    test_frontier_set_get();
    test_frontier_overwrite();
    test_frontier_del();
    test_frontier_del_absent();
    test_frontier_all_values();
    test_frontier_many_predicates();

    printf("\n--- Query ---\n");
    test_query_exact_match();
    test_query_variable_match();
    test_query_no_match();
    test_query_empty_frontier();
    test_query_arg_count_mismatch();

    printf("\n--- Script loading ---\n");
    test_load_empty();
    test_load_whitespace_only();
    test_load_comments_only();
    test_load_single_intent();
    test_load_intent_with_params();
    test_load_multiple_intents();
    test_load_syntax_error();
    test_load_missing_end();
    test_intent_get_nonexistent();
    test_script_free_null();
    test_frontier_free_null();
    test_load_file_nonexistent();
test_exec_param_as_predicate_name();

    printf("\n--- Expression evaluation ---\n");
    test_exec_true_literal();
    test_exec_false_literal();
    test_exec_both_literal();
    test_exec_not_true();
    test_exec_not_false();
    test_exec_not_both();
    test_exec_and_truth_table();
    test_exec_or_truth_table();
    test_exec_precedence();
    test_exec_parens_override();
    test_exec_predicate_lookup();
    test_exec_predicate_absent_is_false();

    printf("\n--- Parameterized intents ---\n");
    test_exec_param_assignment();
    test_exec_wrong_arg_count();

    printf("\n--- When-blocks ---\n");
    test_when_guard_true();
    test_when_guard_false();
    test_when_predicate_guard();

    printf("\n--- Spec example: propagate_active ---\n");
    test_propagate_active();

    printf("\n--- Convergence ---\n");
    test_convergence_no_change();

    printf("\n--- Assignment resolution ---\n");
    test_resolution_true_false();
    test_resolution_both_everything();

    printf("\n--- Edge cases ---\n");
    test_nested_when();
    test_single_param_zero_arg();
    test_frontier_set_null_frontier();
    test_frontier_get_null_frontier();
    test_frontier_set_null_name();
    test_result_free_null();

test_frontier_set_temporary_name();
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_ok, tests_run);

    return (tests_ok == tests_run) ? 0 : 1;
}
