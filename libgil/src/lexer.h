/* lexer.h -- Gil source tokenizer (internal)
 *
 * The lexer converts a raw Gil source string into a flat array of
 * tokens. Tokens are heap-allocated and freed with lexer_free().
 * All identifier strings are interned through the provided Intern table.
 */

#ifndef GIL_LEXER_H
#define GIL_LEXER_H

#include <stddef.h> /* size_t */
#include "intern.h"

/* Token kinds */
enum {
    TOK_EOF = 0,
    TOK_INTENT,      /* "intent"    */
    TOK_WHEN,        /* "when"      */
    TOK_DO,          /* "do"        */
    TOK_END,         /* "end"       */
    TOK_AND,         /* "and"       */
    TOK_OR,          /* "or"        */
    TOK_NOT,         /* "not"       */
    TOK_TRUE,        /* "true"      */
    TOK_FALSE,       /* "false"     */
    TOK_BOTH,        /* "both"      */
    TOK_ASSIGN,      /* "<="        */
    TOK_COMMA,       /* ","         */
    TOK_LPAREN,      /* "("         */
    TOK_RPAREN,      /* ")"         */
    TOK_LBRACKET,    /* "["         */
    TOK_RBRACKET,    /* "]"         */
    TOK_IDENT        /* identifier  */
};

typedef struct Token {
    int kind;
    int line;                /* 1-based source line number       */
    const char *text;        /* for TOK_IDENT: interned string   */
} Token;

/* Token array produced by the lexer. */
typedef struct TokenList {
    Token *data;
    size_t count;
    size_t capacity;
} TokenList;

/* Lex source into a TokenList. All ident text is interned. Returns
   0 on success, -1 on failure (memory). */
int lexer_tokenize(const char *source, Intern *intern, TokenList *out);

/* Free tokens and the list (does NOT free interned strings). */
void lexer_free(TokenList *list);

#endif /* GIL_LEXER_H */