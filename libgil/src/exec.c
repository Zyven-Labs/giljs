/* exec.c -- intent executor: convergence loop with atomic commit per iteration */

#include "gil.h"
#include "ast.h"
#include "frontier.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Binding environment                                                */
/* ------------------------------------------------------------------ */

#define MAX_ENV 64

typedef struct {
    const char *names[MAX_ENV];    /* variable names (interned)       */
    const char *values[MAX_ENV];   /* concrete values                 */
    int         count;
} Env;

static void env_init(Env *e) { e->count = 0; }

static void env_copy(Env *dst, Env *src)
{
    int i;
    dst->count = src->count;
    for (i = 0; i < src->count; i++) {
        dst->names[i] = src->names[i];
        dst->values[i] = src->values[i];
    }
}

static int env_put(Env *e, const char *name, const char *value)
{
    int i;
    for (i = 0; i < e->count; i++)
        if (e->names[i] == name) {
            e->values[i] = value;
            return 0;
        }
    if (e->count >= MAX_ENV) return -1;
    e->names[e->count] = name;
    e->values[e->count] = value;
    e->count++;
    return 0;
}

static const char* env_get(Env *e, const char *name)
{
    int i;
    for (i = 0; i < e->count; i++)
        if (e->names[i] == name) return e->values[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Assignment accumulator                                             */
/* ------------------------------------------------------------------ */

/* We store pending assignments (predicate -> GilVal) for one iteration.
   A simple linked list or dynamic array. Multiple assignments to the
   same predicate are merged via resolution rules.                     */

typedef struct AssignEntry {
    const char *pred_name;
    const char **pred_args;
    size_t      pred_argc;
    GilVal      value;
    int         args_owned;   /* 1 if pred_args were malloc'd          */
} AssignEntry;

typedef struct {
    AssignEntry *entries;
    size_t       count;
    size_t       capacity;
} AssignBuf;

static void assignbuf_init(AssignBuf *buf)
{
    buf->entries = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

static void assignbuf_free(AssignBuf *buf)
{
    size_t i;
    if (buf->entries) {
        for (i = 0; i < buf->count; i++)
            if (buf->entries[i].args_owned)
                free((void*)buf->entries[i].pred_args);
        free(buf->entries);
    }
    buf->entries = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

static int assignbuf_add(AssignBuf *buf, const char *name,
                         const char *args[], size_t argc, GilVal val,
                         int args_owned)
{
    if (buf->count >= buf->capacity) {
        size_t nc = buf->capacity ? buf->capacity * 2 : 32;
        AssignEntry *tmp = (AssignEntry*)realloc(buf->entries,
            nc * sizeof(AssignEntry));
        if (!tmp) return -1;
        buf->entries = tmp;
        buf->capacity = nc;
    }
    buf->entries[buf->count].pred_name = name;
    buf->entries[buf->count].pred_args = args;
    buf->entries[buf->count].pred_argc = argc;
    buf->entries[buf->count].value = val;
    buf->entries[buf->count].args_owned = args_owned;
    buf->count++;
    return 0;
}

/* Assignment resolution per spec Section 10 */
static GilVal resolve_vals(GilVal a, GilVal b)
{
    if (a == GIL_BOTH || b == GIL_BOTH) return GIL_BOTH;
    if (a == GIL_TRUE && b == GIL_FALSE) return GIL_BOTH;
    if (a == GIL_FALSE && b == GIL_TRUE) return GIL_BOTH;
    if (a == GIL_TRUE || b == GIL_TRUE)  return GIL_TRUE;
    return GIL_FALSE;
}

/* Resolve all accumulated assignments: merge entries targeting the
   same predicate and produce a final list. Returns -1 on OOM. */
static int assignbuf_resolve(AssignBuf *buf)
{
    size_t i, j;
    for (i = 0; i < buf->count; i++) {
        for (j = i + 1; j < buf->count; ) {
            AssignEntry *a = &buf->entries[i];
            AssignEntry *b = &buf->entries[j];
            if (a->pred_argc == b->pred_argc &&
                strcmp(a->pred_name, b->pred_name) == 0) {
                size_t k;
                int same = 1;
                for (k = 0; k < a->pred_argc; k++) {
                    if (strcmp(a->pred_args[k], b->pred_args[k]) != 0) {
                        same = 0; break;
                    }
                }
                if (same) {
                    a->value = resolve_vals(a->value, b->value);
                    if (b->args_owned)
                        free((void*)b->pred_args);
                    /* Remove b by swapping with last. */
                    buf->entries[j] = buf->entries[buf->count - 1];
                    buf->count--;
                    continue;
                }
            }
            j++;
        }
    }
    return 0;
}
/* ------------------------------------------------------------------ */
/* Expression evaluation                                                */
/* ------------------------------------------------------------------ */

static int is_var(const char *s) { return s && isupper((unsigned char)s[0]); }

/* Recursively collect all variable names appearing in an expression. */
static void expr_collect_vars(Exp *e, Env *vars)
{
    if (!e) return;
    if (e->kind == EXP_PRED) {
        int k;
        for (k = 0; k < e->u.pred.argc; k++) {
            if (is_var(e->u.pred.args[k]))
                env_put(vars, e->u.pred.args[k], NULL);
        }
    } else if (e->kind == EXP_NOT) {
        expr_collect_vars(e->u.unop.sub, vars);
    } else if (e->kind == EXP_AND || e->kind == EXP_OR) {
        expr_collect_vars(e->u.binop.left, vars);
        expr_collect_vars(e->u.binop.right, vars);
    }
}

/* Evaluate an expression with given bindings and frontier. */
static GilVal expr_eval(Exp *e, Env *env, const GilFrontier *frontier)
{
    if (!e) return GIL_FALSE;
    switch (e->kind) {
    case EXP_VALUE:
        return (GilVal)e->u.value;

    case EXP_PRED: {
        /* Resolve any variables in predicate args to concrete values. */
        const char **resolved;
        int  i;
        int  argc = e->u.pred.argc;
        GilVal val;

        if (argc == 0) {
            return gil_frontier_get(frontier, e->u.pred.name, NULL, 0);
        }
        resolved = (const char**)malloc((size_t)argc * sizeof(const char*));
        if (!resolved) return GIL_FALSE;
        for (i = 0; i < argc; i++) {
            if (is_var(e->u.pred.args[i]))
                resolved[i] = env_get(env, e->u.pred.args[i]);
            else
                resolved[i] = e->u.pred.args[i];
            if (!resolved[i]) { free(resolved); return GIL_FALSE; }
        }
        val = gil_frontier_get(frontier, e->u.pred.name, resolved, (size_t)argc);
        free(resolved);
        return val;
    }

    case EXP_NOT: {
        GilVal v = expr_eval(e->u.unop.sub, env, frontier);
        if (v == GIL_TRUE)  return GIL_FALSE;
        if (v == GIL_FALSE) return GIL_TRUE;
        return GIL_BOTH;
    }

    case EXP_AND: {
        GilVal l = expr_eval(e->u.binop.left, env, frontier);
        /* Short-circuit on false */
        if (l == GIL_FALSE) return GIL_FALSE;
        {
            GilVal r = expr_eval(e->u.binop.right, env, frontier);
            if (l == GIL_TRUE && r == GIL_TRUE) return GIL_TRUE;
            if (r == GIL_FALSE) return GIL_FALSE;
            return GIL_BOTH;
        }
    }

    case EXP_OR: {
        GilVal l = expr_eval(e->u.binop.left, env, frontier);
        if (l == GIL_TRUE) return GIL_TRUE;
        {
            GilVal r = expr_eval(e->u.binop.right, env, frontier);
            if (r == GIL_TRUE) return GIL_TRUE;
            if (l == GIL_FALSE && r == GIL_FALSE) return GIL_FALSE;
            return GIL_BOTH;
        }
    }

    default:
        return GIL_FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* When-block variable discovery and execution                         */
/* ------------------------------------------------------------------ */

/* Forward */
static int exec_stmts(const Stmt *stmts, int count, Env *env,
                      const GilFrontier *frontier, AssignBuf *buf);

/* Execute a when-block: find all variable bindings that satisfy the
   condition, and for each, evaluate the body. */
static int exec_when(const Stmt *when, Env *outer_env,
                     const GilFrontier *frontier, AssignBuf *buf)
{
    Exp *cond = when->when.cond;
    Env  cond_vars;

    env_init(&cond_vars);
    expr_collect_vars(cond, &cond_vars);

    /* If no variables in the condition, simple guard. */
    if (cond_vars.count == 0) {
        GilVal v = expr_eval(cond, outer_env, frontier);
        if (v == GIL_TRUE || v == GIL_BOTH)
            return exec_stmts(when->when.body, when->when.body_count,
                              outer_env, frontier, buf);
        return 0;
    }

    /* Condition has variables. Brute-force: iterate all frontier
       predicates to find each variable's possible concrete values.
       This works correctly for simple pattern-matching when-blocks
       but is O(frontier_size * domain_size^N) in the worst case.
       For Gil v1, frontier sizes are expected to be modest. */

    /* Collect candidate values for each unbound variable. */
    {
        const char **cands[MAX_ENV];
        int          cand_counts[MAX_ENV];
        int          cand_caps[MAX_ENV];
        int          vi;

        for (vi = 0; vi < cond_vars.count; vi++) {
            cands[vi] = NULL;
            cand_counts[vi] = 0;
            cand_caps[vi] = 0;

            /* Scan frontier for this variable appearing in the condition. */
            {
                size_t si;
                for (si = 0; si < frontier->capacity; si++) {
                    PredSlot *slot = &frontier->slots[si];
                    if (!slot->occupied) continue;
                    /* Check if this slot's name appears as a predicate
                       name in the condition with this variable as an arg. */
                    /* Simple heuristic: scan all predicate expressions in
                       the condition. If cond has pred(..., var, ...) where
                       var matches cond_vars.names[vi], add slot's arg. */
                    /* For now: just collect all arg values from all matching
                       predicate names in the condition. This is a simple
                       but correct approach for Gil v1. */
                    {
                        /* Find all predicate sub-exprs in condition. */
                        /* Simplified: try to evaluate with this variable
                           bound to each possible value from frontier. */
                        size_t ak;
                        for (ak = 0; ak < slot->argc; ak++) {
                            /* Add slot->args[ak] as a candidate. */
                            int already = 0;
                            int ci;
                            for (ci = 0; ci < cand_counts[vi]; ci++) {
                                if (strcmp(cands[vi][ci], slot->args[ak]) == 0)
                                    { already = 1; break; }
                            }
                            if (!already) {
                                if (cand_counts[vi] >= cand_caps[vi]) {
                                    int nc = cand_caps[vi] ? cand_caps[vi]*2 : 8;
                                    const char **t = (const char**)realloc(
                                        cands[vi], nc * sizeof(const char*));
                                    if (!t) goto cleanup_cands;
                                    cands[vi] = t; cand_caps[vi] = nc;
                                }
                                cands[vi][cand_counts[vi]++] = slot->args[ak];
                            }
                        }
                    }
                }
            }
        }

        /* Now enumerate all binding combinations. If any variable has
           no candidates, no bindings possible. */
        {
            int *indices;
            int done = 0;

            for (vi = 0; vi < cond_vars.count; vi++)
                if (cand_counts[vi] == 0) done = 1;

            {
                size_t n;
                n = (cond_vars.count > 0)
                  ? (size_t)(cond_vars.count) * sizeof(int) : 0;
                indices = (int*)malloc(n);
                if (indices) {
                    int zi;
                    for (zi = 0; zi < cond_vars.count; zi++)
                        indices[zi] = 0;
                }
            }
            if (!indices) done = 1;

            while (!done) {
                Env  test_env;
                GilVal result;

                env_copy(&test_env, outer_env);
                for (vi = 0; vi < cond_vars.count; vi++)
                    env_put(&test_env, cond_vars.names[vi],
                            cands[vi][indices[vi]]);

                result = expr_eval(cond, &test_env, frontier);
                if (result == GIL_TRUE || result == GIL_BOTH) {
                    if (exec_stmts(when->when.body, when->when.body_count,
                                   &test_env, frontier, buf) != 0) {
                        free(indices);
                        goto cleanup_cands;
                    }
                }

                /* Advance combination counter. */
                {
                    int pos = cond_vars.count - 1;
                    while (pos >= 0) {
                        indices[pos]++;
                        if (indices[pos] < cand_counts[pos]) break;
                        indices[pos] = 0;
                        pos--;
                    }
                    if (pos < 0) done = 1;
                }
            }
            free(indices);
        }

    cleanup_cands:
        for (vi = 0; vi < cond_vars.count; vi++) free(cands[vi]);
    }
    return 0;
}

/* Execute an assignment: evaluate RHS, produce pending assignment.
   Predicate name and arguments with variables are resolved against the env. */
static int exec_assign(const Stmt *stmt, Env *env,
                       const GilFrontier *frontier, AssignBuf *buf)
{
    GilVal val = expr_eval(stmt->assign.rhs, env, frontier);
    const char *pred_name;
    int has_vars;

    /* Resolve the predicate name itself — it may be a variable. */
    if (is_var(stmt->assign.name))
        pred_name = env_get(env, stmt->assign.name);
    else
        pred_name = stmt->assign.name;
    if (!pred_name) return -1;

    /* Resolve predicate args: replace variable names with concrete values.
       We build a resolved args array on the heap; it will be freed when
       the assignbuf is freed. */
    {
        const char **resolved;
        int i;

        has_vars = 0;
        for (i = 0; i < stmt->assign.argc; i++) {
            if (is_var(stmt->assign.args[i])) { has_vars = 1; break; }
        }
        if (!has_vars) {
            return assignbuf_add(buf, pred_name,
                stmt->assign.args, (size_t)stmt->assign.argc, val, 0);
        }
        resolved = (const char**)malloc((size_t)stmt->assign.argc * sizeof(const char*));
        if (!resolved) return -1;
        for (i = 0; i < stmt->assign.argc; i++) {
            if (is_var(stmt->assign.args[i]))
                resolved[i] = env_get(env, stmt->assign.args[i]);
            else
                resolved[i] = stmt->assign.args[i];
            if (!resolved[i]) { free(resolved); return -1; }
        }
        return assignbuf_add(buf, pred_name,
            resolved, (size_t)stmt->assign.argc, val, 1);
    }
}

/* Execute an array of statements. */
static int exec_stmts(const Stmt *stmts, int count, Env *env,
                      const GilFrontier *frontier, AssignBuf *buf)
{
    int i;
    for (i = 0; i < count; i++) {
        if (stmts[i].kind == STMT_ASSIGN) {
            if (exec_assign(&stmts[i], env, frontier, buf) != 0)
                return -1;
        } else {
            /* STMT_WHEN */
            if (exec_when(&stmts[i], env, frontier, buf) != 0)
                return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Convergence loop and public API                                     */
/* ------------------------------------------------------------------ */

int gil_intent_execute(GilIntent *intent, GilFrontier *frontier,
                       const char *args[], size_t argc)
{
    Intent *it;
    Env     env;

    if (!intent || !frontier) return -1;

    /* Cast opaque handle to internal type. */
    it = (Intent*)intent;

    /* Check argument count. */
    if ((size_t)it->param_count != argc) return -1;

    /* Bind parameters. */
    env_init(&env);
    {
        int i;
        for (i = 0; i < it->param_count; i++)
            env_put(&env, it->params[i], args[i]);
    }

    /* Convergence loop. */
    for (;;) {
        AssignBuf buf;
        size_t    i;
        int       changed = 0;

        assignbuf_init(&buf);

        /* Evaluate all statements, collect assignments.
           Frontier is NOT modified during evaluation. */
        if (exec_stmts(it->stmts, it->stmt_count,
                       &env, frontier, &buf) != 0) {
            assignbuf_free(&buf);
            return -1;
        }

        /* If no assignments, converged. */
        if (buf.count == 0) {
            assignbuf_free(&buf);
            return 0;
        }

        /* Resolve conflicts. */
        if (assignbuf_resolve(&buf) != 0) {
            assignbuf_free(&buf);
            return -1;
        }

        /* Lock frontier, commit atomically, unlock. */
        gil_frontier_lock(frontier);
        for (i = 0; i < buf.count; i++) {
            AssignEntry *e = &buf.entries[i];
            GilVal old = gil_frontier_get(frontier, e->pred_name,
                (const char**)e->pred_args, e->pred_argc);
            GilVal neu = e->value;
            if (old != neu) {
                gil_frontier_set(frontier, e->pred_name,
                    (const char**)e->pred_args, e->pred_argc, neu);
                changed = 1;
            }
        }
        gil_frontier_unlock(frontier);

        assignbuf_free(&buf);

        /* If nothing changed, converged. */
        if (!changed) return 0;
    }
}
