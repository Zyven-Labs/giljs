/* parser.c -- Gil recursive-descent parser implementation */

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Error buffer                                                       */
/* ------------------------------------------------------------------ */

static char parser_error[256];

static const char* make_error(int line, const char *fmt)
{
    /* In C89 we lack snprintf; sprintf with a large enough buffer is safe
       since error messages are bounded. */
    sprintf(parser_error, "line %d: %s", line, fmt);
    return parser_error;
}

/* ------------------------------------------------------------------ */
/* Token stream iterator                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    Token  *tokens;
    size_t  pos;
    size_t  count;
} TokIter;

static void tok_init(TokIter *it, TokenList *list)
{
    it->tokens = list->data;
    it->pos    = 0;
    it->count  = list->count;
}

static Token* tok_cur(TokIter *it)
{
    if (it->pos >= it->count) return &it->tokens[it->count - 1];
    return &it->tokens[it->pos];
}

static Token* tok_eat(TokIter *it)
{
    Token *t = tok_cur(it);
    if (it->pos < it->count) it->pos++;
    return t;
}

static int tok_kind(TokIter *it) { return tok_cur(it)->kind; }
static int tok_line(TokIter *it) { return tok_cur(it)->line; }
#define LINE(it) tok_line(it)

static Token* tok_expect(TokIter *it, int kind)
{
    Token *t = tok_cur(it);
    if (t->kind == kind) { it->pos++; return t; }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Variable binding scope                                             */
/* ------------------------------------------------------------------ */

#define MAX_BINDINGS 128

typedef struct {
    const char *vars[MAX_BINDINGS];
    int         count;
} Bindings;

static void bindings_init(Bindings *b) { b->count = 0; }

static int bindings_add(Bindings *b, const char *name)
{
    if (b->count >= MAX_BINDINGS) return -1;
    b->vars[b->count++] = name;
    return 0;
}

/* bindings_has and is_variable are used by exec.c (declared there) */

/* ------------------------------------------------------------------ */
/* Forward declarations                                                    */
/* ------------------------------------------------------------------ */
static void exp_free(Exp *e);
static void stmt_free_one(Stmt *s);
static void intent_free_one(Intent *it);

/* ------------------------------------------------------------------ */
/* Expression parsing                                                    */
/* ------------------------------------------------------------------ */

static Exp* parse_expression(TokIter *it, Bindings *bindings,
                             const char **error);

static Exp* parse_predicate(TokIter *it, Bindings *bindings,
                            const char **error)
{
    Exp *exp;
    Token *name_tok;
    const char *name;

    (void)bindings; /* suppress unused parameter warning */

    name_tok = tok_expect(it, TOK_IDENT);
    if (!name_tok) {
        *error = make_error(LINE(it), "expected identifier");
        return NULL;
    }
    name = name_tok->text;

    exp = (Exp*)calloc(1, sizeof(Exp));
    if (!exp) { *error = make_error(LINE(it), "out of memory"); return NULL; }
    exp->kind = EXP_PRED;
    exp->u.pred.name = name;
    exp->u.pred.argc = 0;
    exp->u.pred.args = NULL;

    if (tok_kind(it) == TOK_LBRACKET) {
        const char **args = NULL;
        int argc = 0, cap = 0;
        tok_eat(it);
        while (tok_kind(it) != TOK_RBRACKET) {
            Token *at;
            if (tok_kind(it) == TOK_COMMA) { tok_eat(it); continue; }
            at = tok_expect(it, TOK_IDENT);
            if (!at) { free(args); free(exp);
                *error = make_error(LINE(it), "expected ident in arg list");
                return NULL; }
            if (argc >= cap) {
                int nc = cap ? cap * 2 : 4;
                const char **t = (const char**)realloc(args,
                    nc * sizeof(const char*));
                if (!t) { free(args); free(exp);
                    *error = make_error(LINE(it), "out of memory");
                    return NULL; }
                args = t; cap = nc;
            }
            args[argc++] = at->text;
        }
        tok_eat(it); /* ']' */
        exp->u.pred.argc = argc;
        exp->u.pred.args = args;
    }
    return exp;
}

static Exp* parse_primary(TokIter *it, Bindings *bindings,
                          const char **error)
{
    Exp *exp;
    int val = -1;
    if (tok_kind(it) == TOK_TRUE)  val = 1;
    else if (tok_kind(it) == TOK_FALSE) val = 0;
    else if (tok_kind(it) == TOK_BOTH)  val = 2;
    if (val >= 0) {
        tok_eat(it);
        exp = (Exp*)calloc(1, sizeof(Exp));
        if (!exp) { *error = make_error(LINE(it), "out of memory"); return NULL; }
        exp->kind = EXP_VALUE;
        exp->u.value = val;
        return exp;
    }
    if (tok_kind(it) == TOK_IDENT)
        return parse_predicate(it, bindings, error);
    if (tok_kind(it) == TOK_LPAREN) {
        tok_eat(it);
        exp = parse_expression(it, bindings, error);
        if (!exp) return NULL;
        if (!tok_expect(it, TOK_RPAREN)) {
            exp_free(exp);
            *error = make_error(LINE(it), "expected ')'");
            return NULL;
        }
        return exp;
    }
    *error = make_error(LINE(it), "expected expression");
    return NULL;
}

static Exp* parse_negation(TokIter *it, Bindings *bindings,
                           const char **error)
{
    if (tok_kind(it) == TOK_NOT) {
        Exp *exp, *sub;
        tok_eat(it);
        sub = parse_negation(it, bindings, error);
        if (!sub) return NULL;
        exp = (Exp*)calloc(1, sizeof(Exp));
        if (!exp) { exp_free(sub);
            *error = make_error(LINE(it), "out of memory"); return NULL; }
        exp->kind = EXP_NOT;
        exp->u.unop.sub = sub;
        return exp;
    }
    return parse_primary(it, bindings, error);
}

static Exp* parse_conjunction(TokIter *it, Bindings *bindings,
                              const char **error)
{
    Exp *left = parse_negation(it, bindings, error);
    if (!left) return NULL;
    while (tok_kind(it) == TOK_AND) {
        Exp *right, *e;
        tok_eat(it);
        right = parse_negation(it, bindings, error);
        if (!right) { exp_free(left); return NULL; }
        e = (Exp*)calloc(1, sizeof(Exp));
        if (!e) { exp_free(left); exp_free(right);
            *error = make_error(LINE(it), "out of memory"); return NULL; }
        e->kind = EXP_AND;
        e->u.binop.left = left;
        e->u.binop.right = right;
        left = e;
    }
    return left;
}

static Exp* parse_disjunction(TokIter *it, Bindings *bindings,
                              const char **error)
{
    Exp *left = parse_conjunction(it, bindings, error);
    if (!left) return NULL;
    while (tok_kind(it) == TOK_OR) {
        Exp *right, *e;
        tok_eat(it);
        right = parse_conjunction(it, bindings, error);
        if (!right) { exp_free(left); return NULL; }
        e = (Exp*)calloc(1, sizeof(Exp));
        if (!e) { exp_free(left); exp_free(right);
            *error = make_error(LINE(it), "out of memory"); return NULL; }
        e->kind = EXP_OR;
        e->u.binop.left = left;
        e->u.binop.right = right;
        left = e;
    }
    return left;
}

static Exp* parse_expression(TokIter *it, Bindings *bindings,
                             const char **error)
{
    return parse_disjunction(it, bindings, error);
}

/* ------------------------------------------------------------------ */
/* Statement parsing                                                     */
/* ------------------------------------------------------------------ */

static Stmt* parse_statement(TokIter *it, Bindings *bindings,
                             const char **error);

static Stmt* parse_when(TokIter *it, Bindings *bindings,
                        const char **error)
{
    Stmt *stmt;
    Exp  *cond;
    Stmt *body = NULL;
    int   body_cap = 4, body_count = 0;

    cond = parse_expression(it, bindings, error);
    if (!cond) return NULL;

    if (!tok_expect(it, TOK_DO)) {
        exp_free(cond);
        *error = make_error(LINE(it), "expected 'do' after when");
        return NULL;
    }

    body = (Stmt*)calloc((size_t)body_cap, sizeof(Stmt));
    if (!body) { exp_free(cond);
        *error = make_error(LINE(it), "out of memory"); return NULL; }

    while (tok_kind(it) != TOK_END) {
        Stmt *child = parse_statement(it, bindings, error);
        if (!child) break;
        if (body_count >= body_cap) {
            body_cap *= 2;
            {
                Stmt *tmp = (Stmt*)realloc(body,
                    (size_t)body_cap * sizeof(Stmt));
                if (!tmp) {
                    stmt_free_one(child); free(child); free(body);
                    exp_free(cond);
                    *error = make_error(LINE(it), "out of memory");
                    return NULL;
                }
                body = tmp;
            }
        }
        body[body_count++] = *child;
        free(child);
    }
    if (!tok_expect(it, TOK_END)) {
        int j;
        for (j = 0; j < body_count; j++) stmt_free_one(&body[j]);
        free(body); exp_free(cond);
        *error = make_error(LINE(it), "expected 'end'");
        return NULL;
    }

    stmt = (Stmt*)calloc(1, sizeof(Stmt));
    if (!stmt) {
        int j;
        for (j = 0; j < body_count; j++) stmt_free_one(&body[j]);
        free(body); exp_free(cond);
        *error = make_error(LINE(it), "out of memory");
        return NULL;
    }
    stmt->kind = STMT_WHEN;
    stmt->when.cond = cond;
    stmt->when.body_count = body_count;
    stmt->when.body = body;
    return stmt;
}

static Stmt* parse_assignment(TokIter *it, Bindings *bindings,
                              const char **error)
{
    Stmt *stmt;
    Exp  *target_exp;
    Exp  *rhs;

    target_exp = parse_predicate(it, bindings, error);
    if (!target_exp) return NULL;

    if (!tok_expect(it, TOK_ASSIGN)) {
        exp_free(target_exp);
        *error = make_error(LINE(it), "expected '<='");
        return NULL;
    }

    rhs = parse_expression(it, bindings, error);
    if (!rhs) { exp_free(target_exp); return NULL; }

    stmt = (Stmt*)calloc(1, sizeof(Stmt));
    if (!stmt) { exp_free(target_exp); exp_free(rhs);
        *error = make_error(LINE(it), "out of memory"); return NULL; }
    stmt->kind = STMT_ASSIGN;
    stmt->assign.name = target_exp->u.pred.name;
    stmt->assign.argc = target_exp->u.pred.argc;
    stmt->assign.args = target_exp->u.pred.args;
    stmt->assign.rhs  = rhs;
    free(target_exp);
    return stmt;
}

static Stmt* parse_statement(TokIter *it, Bindings *bindings,
                             const char **error)
{
    if (tok_kind(it) == TOK_WHEN) {
        tok_eat(it);
        return parse_when(it, bindings, error);
    }
    if (tok_kind(it) == TOK_IDENT)
        return parse_assignment(it, bindings, error);
    *error = make_error(LINE(it), "expected statement");
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Intent parsing                                                        */
/* ------------------------------------------------------------------ */

static Intent* parse_intent(TokIter *it, const char **error)
{
    Intent    *intent;
    Bindings   bindings;
    Stmt      *stmts = NULL;
    int        stmt_cap = 8, stmt_count = 0;
    const char **params = NULL;
    int         param_cap = 0, param_count = 0;

    if (!tok_expect(it, TOK_INTENT)) {
        *error = make_error(LINE(it), "expected 'intent'");
        return NULL;
    }

    {
        Token *nt = tok_expect(it, TOK_IDENT);
        if (!nt) {
            *error = make_error(LINE(it), "expected intent name");
            return NULL;
        }
        intent = (Intent*)calloc(1, sizeof(Intent));
        if (!intent) { *error = make_error(LINE(it), "out of memory"); return NULL; }
        intent->name = nt->text;
    }

    bindings_init(&bindings);
    if (!tok_expect(it, TOK_LPAREN)) {
        intent_free_one(intent); free(intent);
        *error = make_error(LINE(it), "expected '('");
        return NULL;
    }

    while (tok_kind(it) != TOK_RPAREN) {
        Token *p;
        if (tok_kind(it) == TOK_COMMA) { tok_eat(it); continue; }
        p = tok_expect(it, TOK_IDENT);
        if (!p) { free(params); intent_free_one(intent); free(intent);
            *error = make_error(LINE(it), "expected param name");
            return NULL; }
        if (param_count >= param_cap) {
            param_cap = param_cap ? param_cap * 2 : 4;
            {
                const char **t = (const char**)realloc(params,
                    (size_t)param_cap * sizeof(const char*));
                if (!t) { free(params); intent_free_one(intent); free(intent);
                    *error = make_error(LINE(it), "out of memory");
                    return NULL; }
                params = t;
            }
        }
        params[param_count++] = p->text;
        bindings_add(&bindings, p->text);
    }
    tok_eat(it);
    intent->params = params;
    intent->param_count = param_count;

    stmts = (Stmt*)calloc((size_t)stmt_cap, sizeof(Stmt));
    if (!stmts) { intent_free_one(intent); free(intent);
        *error = make_error(LINE(it), "out of memory"); return NULL; }

    while (tok_kind(it) != TOK_END && tok_kind(it) != TOK_EOF) {
        Stmt *child = parse_statement(it, &bindings, error);
        if (!child) break;
        if (stmt_count >= stmt_cap) {
            stmt_cap *= 2;
            {
                Stmt *t = (Stmt*)realloc(stmts,
                    (size_t)stmt_cap * sizeof(Stmt));
                if (!t) { stmt_free_one(child); free(child); free(stmts);
                    intent_free_one(intent); free(intent);
                    *error = make_error(LINE(it), "out of memory");
                    return NULL; }
                stmts = t;
            }
        }
        stmts[stmt_count++] = *child;
        free(child);
    }
    if (!tok_expect(it, TOK_END)) {
        int j;
        for (j = 0; j < stmt_count; j++) stmt_free_one(&stmts[j]);
        free(stmts); intent_free_one(intent); free(intent);
        *error = make_error(LINE(it), "expected 'end' to close intent");
        return NULL;
    }

    intent->stmt_count = stmt_count;
    intent->stmts = stmts;
    return intent;
}

/* ------------------------------------------------------------------ */
/* AST free functions                                                     */
/* ------------------------------------------------------------------ */

static void stmt_free_one(Stmt *s)
{
    int i;
    if (!s) return;
    if (s->kind == STMT_ASSIGN) {
        free(s->assign.args);
        if (s->assign.rhs) exp_free(s->assign.rhs);
    } else {
        if (s->when.cond) exp_free(s->when.cond);
        for (i = 0; i < s->when.body_count; i++)
            stmt_free_one(&s->when.body[i]);
        free(s->when.body);
    }
}

static void exp_free(Exp *e)
{
    if (!e) return;
    if (e->kind == EXP_PRED) {
        free(e->u.pred.args);
    } else if (e->kind == EXP_NOT) {
        exp_free(e->u.unop.sub);
    } else if (e->kind == EXP_AND || e->kind == EXP_OR) {
        exp_free(e->u.binop.left);
        exp_free(e->u.binop.right);
    }
    free(e);
}

static void intent_free_one(Intent *it)
{
    int i;
    if (!it) return;
    free(it->params);
    for (i = 0; i < it->stmt_count; i++)
        stmt_free_one(&it->stmts[i]);
    free(it->stmts);
}

/* ------------------------------------------------------------------ */
/* Public interface                                                       */
/* ------------------------------------------------------------------ */

Intent* parser_parse(TokenList *tokens, Intern *intern,
                     int *intent_count, const char **error)
{
    TokIter  it;
    Intent  *intents = NULL;
    int      cap = 8, count = 0;

    (void)intern;
    tok_init(&it, tokens);

    intents = (Intent*)calloc((size_t)cap, sizeof(Intent));
    if (!intents) { *error = "out of memory"; return NULL; }

    while (tok_kind(&it) != TOK_EOF) {
        Intent *parsed;
        if (tok_kind(&it) == TOK_END) { tok_eat(&it); continue; }

        parsed = parse_intent(&it, error);
        if (!parsed) {
            int j;
            for (j = 0; j < count; j++) intent_free_one(&intents[j]);
            free(intents);
            return NULL;
        }
        if (count >= cap) {
            cap *= 2;
            {
                Intent *t = (Intent*)realloc(intents,
                    (size_t)cap * sizeof(Intent));
                if (!t) {
                    int j;
                    for (j = 0; j < count; j++)
                        intent_free_one(&intents[j]);
                    free(intents);
                    intent_free_one(parsed); free(parsed);
                    *error = "out of memory"; return NULL;
                }
                intents = t;
            }
        }
        intents[count++] = *parsed;
        free(parsed);
    }

    *intent_count = count;
    return intents;
}

void parser_free_intents(Intent *intents, int count)
{
    int i;
    for (i = 0; i < count; i++)
        intent_free_one(&intents[i]);
    free(intents);
}
