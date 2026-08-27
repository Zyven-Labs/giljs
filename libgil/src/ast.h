/* ast.h -- abstract syntax tree types (internal)
 *
 * All identifiers in the AST are interned strings — pointer comparison
 * works as string equality. The AST is owned by GilScript and freed
 * when the script is freed.
 */

#ifndef GIL_AST_H
#define GIL_AST_H

#include <stddef.h> /* size_t */

/* ------------------------------------------------------------------ */
/* Expression nodes                                                   */
/* ------------------------------------------------------------------ */

enum {
    EXP_VALUE = 0,   /* literal true / false / both                    */
    EXP_PRED,        /* predicate lookup in frontier                   */
    EXP_NOT,         /* logical negation                               */
    EXP_AND,         /* logical conjunction                            */
    EXP_OR           /* logical disjunction                            */
};

typedef struct Exp {
    int kind;
    union {
        /* EXP_VALUE:   value is an integer 0=GIL_FALSE, 1=GIL_TRUE, 2=GIL_BOTH */
        int value;

        /* EXP_PRED:    predicate(name, args...)                          */
        struct {
            const char *name;          /* interned name                  */
            int         argc;
            const char **args;         /* interned argument strings      */
        } pred;

        /* EXP_NOT:     unary sub-expression                              */
        struct {
            struct Exp *sub;
        } unop;

        /* EXP_AND, EXP_OR:  binary sub-expressions                       */
        struct {
            struct Exp *left;
            struct Exp *right;
        } binop;
    } u;
} Exp;

/* ------------------------------------------------------------------ */
/* Statement nodes                                                    */
/* ------------------------------------------------------------------ */

/* A statement is either a direct assignment or a when-block.
 * When-blocks contain a condition expression and a body of statements.
 * Nested when-blocks are represented by sub-statements in the body.   */

enum {
    STMT_ASSIGN = 0,
    STMT_WHEN
};

typedef struct Stmt {
    int kind;
    /* For STMT_ASSIGN */
    struct {
        const char *name;              /* interned predicate name        */
        int         argc;
        const char **args;             /* interned args (may be vars)    */
        Exp        *rhs;               /* right-hand-side expression     */
    } assign;

    /* For STMT_WHEN */
    struct {
        Exp        *cond;              /* when-condition expression      */
        int         body_count;
        struct Stmt *body;             /* array of body statements       */
    } when;
} Stmt;

/* ------------------------------------------------------------------ */
/* Intent                                                             */
/* ------------------------------------------------------------------ */

typedef struct Intent {
    const char *name;                  /* interned intent name           */
    int         param_count;
    const char **params;               /* interned parameter names       */
    int         stmt_count;
    Stmt       *stmts;                 /* array of top-level statements  */
} Intent;

#endif /* GIL_AST_H */