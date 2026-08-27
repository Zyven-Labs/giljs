/* exec.c -- intent executor: convergence loop with atomic commit per iteration */

#include "gil.h"
#include "ast.h"
#include "frontier.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
    int         arr_owned;       /* 1 if pred_args array was malloc'd     */
    int        *arg_owned;       /* per-slot: 1 if pred_args[i] malloc'd  */
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

static void assignentry_free(AssignEntry *e)
{
    if (e->arr_owned) {
        if (e->arg_owned) {
            size_t k;
            for (k = 0; k < e->pred_argc; k++)
                if (e->arg_owned[k]) free((void*)e->pred_args[k]);
            free(e->arg_owned);
        }
        free((void*)e->pred_args);
    }
}

static void assignbuf_free(AssignBuf *buf)
{
    size_t i;
    if (buf->entries) {
        for (i = 0; i < buf->count; i++)
            assignentry_free(&buf->entries[i]);
        free(buf->entries);
    }
    buf->entries = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

static int assignbuf_add(AssignBuf *buf, const char *name,
                         const char *args[], size_t argc, GilVal val,
                         int arr_owned, int *arg_owned)
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
    buf->entries[buf->count].arr_owned = arr_owned;
    buf->entries[buf->count].arg_owned = arg_owned;
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
                    assignentry_free(b);
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

/* Strictly parse a decimal integer string into a long. Accepts an optional
   leading '+' or '-'. Returns 1 on success, 0 if the string is not a valid
   integer. */
static int parse_long_strict(const char *s, long *out)
{
    char *end = NULL;
    long v;
    if (!s || *s == '\0') return 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

/* Recursively collect all variable names appearing in an argument. */
static void arg_collect_vars(const Arg *a, Env *vars)
{
    if (!a) return;
    if (a->kind == ARG_VAR) {
        env_put(vars, a->u.text, NULL);
    } else if (a->kind == ARG_ADD || a->kind == ARG_SUB ||
               a->kind == ARG_MUL || a->kind == ARG_DIV) {
        arg_collect_vars(a->u.binop.left, vars);
        arg_collect_vars(a->u.binop.right, vars);
    }
}

/* Evaluate an argument node to a concrete string.
 *   out       -- receives the string (borrowed or heap).
 *   out_owned -- set to 1 when *out must be freed by the caller.
 * Returns:
 *   0  on success,
 *   -1 on a hard error (unbound variable, out of memory, divide by zero),
 *   1  when an operand is not a valid integer (arithmetic only). */
static int arg_eval(const Arg *a, Env *env, const char **out, int *out_owned)
{
    if (!a) return -1;
    switch (a->kind) {
    case ARG_LITERAL:
        *out = a->u.text;
        *out_owned = 0;
        return 0;

    case ARG_VAR: {
        const char *v = env_get(env, a->u.text);
        if (!v) return -1;
        *out = v;
        *out_owned = 0;
        return 0;
    }

    case ARG_ADD: case ARG_SUB: case ARG_MUL: case ARG_DIV: {
        const char *ls = NULL, *rs = NULL;
        int lo = 0, ro = 0;
        long l, r, v;
        int lr = arg_eval(a->u.binop.left, env, &ls, &lo);
        int rr;
        if (lr != 0) return lr;
        rr = arg_eval(a->u.binop.right, env, &rs, &ro);
        if (rr != 0) {
            if (lo) free((void*)ls);
            return rr;
        }
        if (!parse_long_strict(ls, &l) || !parse_long_strict(rs, &r)) {
            if (lo) free((void*)ls);
            if (ro) free((void*)rs);
            return 1; /* non-integer operand */
        }
        if (a->kind == ARG_DIV && r == 0) {
            if (lo) free((void*)ls);
            if (ro) free((void*)rs);
            return -1;
        }
        if (a->kind == ARG_ADD)      v = l + r;
        else if (a->kind == ARG_SUB) v = l - r;
        else if (a->kind == ARG_MUL) v = l * r;
        else                         v = l / r;
        if (lo) free((void*)ls);
        if (ro) free((void*)rs);
        {
            char buf[32];
            char *s;
            sprintf(buf, "%ld", v);
            s = (char*)malloc(strlen(buf) + 1);
            if (!s) return -1;
            strcpy(s, buf);
            *out = s;
            *out_owned = 1;
        }
        return 0;
    }
    }
    return -1;
}

/* Recursively collect all variable names appearing in an expression. */
static void expr_collect_vars(Exp *e, Env *vars)
{
    if (!e) return;
    if (e->kind == EXP_PRED) {
        int k;
        for (k = 0; k < e->u.pred.argc; k++)
            arg_collect_vars(e->u.pred.args[k], vars);
    } else if (e->kind == EXP_NOT) {
        expr_collect_vars(e->u.unop.sub, vars);
    } else if (e->kind == EXP_AND || e->kind == EXP_OR) {
        expr_collect_vars(e->u.binop.left, vars);
        expr_collect_vars(e->u.binop.right, vars);
    }
}

/* Evaluate an expression with given bindings and frontier.
   Returns 0 on success (stores the result in *out), -1 on error. */
static int expr_eval(Exp *e, Env *env, const GilFrontier *frontier,
                     GilVal *out)
{
    if (!e) return -1;
    switch (e->kind) {
    case EXP_VALUE:
        *out = (GilVal)e->u.value;
        return 0;

    case EXP_PRED: {
        const char **resolved;
        int *owned;
        int  i;
        int  argc = e->u.pred.argc;
        GilVal val;

        if (argc == 0) {
            *out = gil_frontier_get(frontier, e->u.pred.name, NULL, 0);
            return 0;
        }
        resolved = (const char**)malloc((size_t)argc * sizeof(const char*));
        owned = (int*)calloc((size_t)argc, sizeof(int));
        if (!resolved || !owned) { free(resolved); free(owned); return -1; }
        for (i = 0; i < argc; i++) {
            int r = arg_eval(e->u.pred.args[i], env, &resolved[i], &owned[i]);
            if (r == 1) {
                int j;
                for (j = 0; j < i; j++) if (owned[j]) free((void*)resolved[j]);
                free(resolved); free(owned);
                *out = GIL_FALSE;
                return 0;
            }
            if (r != 0) {
                int j;
                for (j = 0; j < i; j++) if (owned[j]) free((void*)resolved[j]);
                free(resolved); free(owned);
                return -1;
            }
        }
        val = gil_frontier_get(frontier, e->u.pred.name,
                               (const char**)resolved, (size_t)argc);
        for (i = 0; i < argc; i++) if (owned[i]) free((void*)resolved[i]);
        free(resolved); free(owned);
        *out = val;
        return 0;
    }

    case EXP_NOT: {
        GilVal v;
        if (expr_eval(e->u.unop.sub, env, frontier, &v) != 0) return -1;
        if (v == GIL_TRUE)  *out = GIL_FALSE;
        else if (v == GIL_FALSE) *out = GIL_TRUE;
        else *out = GIL_BOTH;
        return 0;
    }

    case EXP_AND: {
        GilVal l;
        if (expr_eval(e->u.binop.left, env, frontier, &l) != 0) return -1;
        if (l == GIL_FALSE) { *out = GIL_FALSE; return 0; }
        {
            GilVal r;
            if (expr_eval(e->u.binop.right, env, frontier, &r) != 0) return -1;
            if (l == GIL_TRUE && r == GIL_TRUE) *out = GIL_TRUE;
            else if (r == GIL_FALSE) *out = GIL_FALSE;
            else *out = GIL_BOTH;
        }
        return 0;
    }

    case EXP_OR: {
        GilVal l;
        if (expr_eval(e->u.binop.left, env, frontier, &l) != 0) return -1;
        if (l == GIL_TRUE) { *out = GIL_TRUE; return 0; }
        {
            GilVal r;
            if (expr_eval(e->u.binop.right, env, frontier, &r) != 0) return -1;
            if (r == GIL_TRUE) *out = GIL_TRUE;
            else if (l == GIL_FALSE && r == GIL_FALSE) *out = GIL_FALSE;
            else *out = GIL_BOTH;
        }
        return 0;
    }

    default:
        *out = GIL_FALSE;
        return 0;
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

/* Recursively scan an expression for predicate sub-expressions.
   For each predicate arg that is a plain ARG_VAR, record the predicate
   name in the variable's association table. The vps array is indexed
   by variable position in vars, holding up to MAX_ENV pred names each. */
static void scan_cond_preds(Exp *e, Env *vars,
                            const char *(*vps)[MAX_ENV], int *vpc)
{
    int vi, p;
    if (!e) return;
    if (e->kind == EXP_PRED) {
        int k;
        for (k = 0; k < e->u.pred.argc; k++) {
            Arg *a = e->u.pred.args[k];
            if (a->kind == ARG_VAR) {
                for (vi = 0; vi < vars->count; vi++) {
                    if (vars->names[vi] == a->u.text) {
                        int already = 0;
                        for (p = 0; p < vpc[vi]; p++) {
                            if (vps[vi][p] == e->u.pred.name)
                                { already = 1; break; }
                        }
                        if (!already && vpc[vi] < MAX_ENV)
                            vps[vi][vpc[vi]++] = e->u.pred.name;
                        break;
                    }
                }
            }
        }
    } else if (e->kind == EXP_NOT) {
        scan_cond_preds(e->u.unop.sub, vars, vps, vpc);
    } else if (e->kind == EXP_AND || e->kind == EXP_OR) {
        scan_cond_preds(e->u.binop.left, vars, vps, vpc);
        scan_cond_preds(e->u.binop.right, vars, vps, vpc);
    }
}

static int exec_when(const Stmt *when, Env *outer_env,
                     const GilFrontier *frontier, AssignBuf *buf)
{
    Exp *cond = when->when.cond;
    Env  cond_vars;

    env_init(&cond_vars);
    expr_collect_vars(cond, &cond_vars);

    /* Remove variables already bound by outer env (intent params or
       enclosing when clauses). The brute-force enumeration only needs
       to try values for unbound variables. */
    {
        int dst = 0, vi;
        for (vi = 0; vi < cond_vars.count; vi++) {
            if (env_get(outer_env, cond_vars.names[vi]) == NULL)
                cond_vars.names[dst++] = cond_vars.names[vi];
        }
        cond_vars.count = dst;
    }

    /* If no variables in the condition, simple guard. */
    if (cond_vars.count == 0) {
        GilVal v;
        if (expr_eval(cond, outer_env, frontier, &v) != 0)
            return -1;
        if (v == GIL_TRUE || v == GIL_BOTH)
            return exec_stmts(when->when.body, when->when.body_count,
                              outer_env, frontier, buf);
        return 0;
    }

    /* Condition has variables. Brute-force: iterate all frontier
       predicates to find each variable's possible concrete values.
       This works correctly for simple pattern-matching when-blocks
       but is O(frontier_size * domain_size^N) in the worst case. */

    /* Precompute which predicate names each variable appears in
       so we only collect candidates from matching frontier slots. */
    {
        const char *var_preds[MAX_ENV][MAX_ENV];
        int         var_pred_count[MAX_ENV];
        int         vi;

        for (vi = 0; vi < cond_vars.count; vi++)
            var_pred_count[vi] = 0;

        scan_cond_preds(cond, &cond_vars, var_preds, var_pred_count);

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
                    int pi, should_collect = 0;

                    if (!slot->occupied) continue;

                    /* Only collect candidates from frontier slots whose
                       predicate name matches one of the predicate names
                       where this variable appears in the condition. */
                    for (pi = 0; pi < var_pred_count[vi]; pi++) {
                        if (strcmp(slot->name, var_preds[vi][pi]) == 0) {
                            should_collect = 1;
                            break;
                        }
                    }
                    if (!should_collect) continue;

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

                if (expr_eval(cond, &test_env, frontier, &result) != 0) {
                    free(indices);
                    goto cleanup_cands;
                }
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
    }
    return 0;
}

/* Execute an assignment: evaluate RHS, produce pending assignment.
   Predicate name and arguments with variables are resolved against the env. */
static int exec_assign(const Stmt *stmt, Env *env,
                       const GilFrontier *frontier, AssignBuf *buf)
{
    GilVal val;
    const char *pred_name;
    int i;

    if (expr_eval(stmt->assign.rhs, env, frontier, &val) != 0)
        return -1;

    /* Resolve the predicate name itself — it may be a variable. */
    if (is_var(stmt->assign.name))
        pred_name = env_get(env, stmt->assign.name);
    else
        pred_name = stmt->assign.name;
    if (!pred_name) return -1;

    if (stmt->assign.argc == 0)
        return assignbuf_add(buf, pred_name, NULL, 0, val, 0, NULL);

    {
        const char **resolved;
        int *owned;

        resolved = (const char**)malloc((size_t)stmt->assign.argc *
                                        sizeof(const char*));
        owned = (int*)calloc((size_t)stmt->assign.argc, sizeof(int));
        if (!resolved || !owned) { free(resolved); free(owned); return -1; }

        for (i = 0; i < stmt->assign.argc; i++) {
            int r = arg_eval(stmt->assign.args[i], env,
                             &resolved[i], &owned[i]);
            if (r != 0) {
                int j;
                for (j = 0; j < i; j++) if (owned[j]) free((void*)resolved[j]);
                free(resolved); free(owned);
                return -1;
            }
        }
        return assignbuf_add(buf, pred_name, resolved,
                             (size_t)stmt->assign.argc, val, 1, owned);
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
