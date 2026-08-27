/* lexer.c -- Gil source tokenizer implementation */

#include "lexer.h"
#include "intern.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static int tokenlist_push(TokenList *list, int kind, int line,
                          const char *text)
{
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 64;
        Token *new_data;
        new_data = (Token*)realloc(list->data, new_cap * sizeof(Token));
        if (!new_data) return -1;
        list->data = new_data;
        list->capacity = new_cap;
    }
    list->data[list->count].kind = kind;
    list->data[list->count].line = line;
    list->data[list->count].text = text;
    list->count++;
    return 0;
}

static const char* lex_keyword(const char *s, size_t len)
{
    /* Gil keywords in alphabetical order, for linear scan.
       Table is small enough (9 keywords) that a loop is fine. */
    static const char *keywords[] = {
        "and", "both", "do", "end",
        "false", "intent", "not", "or", "true", "when"
    };
    static const int  kw_kinds[] = {
        TOK_AND, TOK_BOTH, TOK_DO, TOK_END,
        TOK_FALSE, TOK_INTENT, TOK_NOT, TOK_OR, TOK_TRUE, TOK_WHEN
    };
    static const int nk = (int)(sizeof(keywords) / sizeof(keywords[0]));
    int i;
    for (i = 0; i < nk; i++) {
        size_t klen = strlen(keywords[i]);
        if (len == klen && memcmp(s, keywords[i], len) == 0)
            return (const char*)(size_t)kw_kinds[i];
    }
    return NULL;
}

int lexer_tokenize(const char *source, Intern *intern, TokenList *out)
{
    const char *p;
    int line = 1;

    out->data = NULL;
    out->count = 0;
    out->capacity = 0;

    if (!source) {
        if (tokenlist_push(out, TOK_EOF, line, NULL) != 0) return -1;
        return 0;
    }

    p = source;
    while (*p) {
        /* Skip whitespace, count newlines. */
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            continue;
        }
        if (*p == '\n') {
            line++;
            p++;
            continue;
        }

        /* Comments: # to end of line (not in the BNF but useful). */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Two-character operators. */
        if (p[0] == '<' && p[1] == '=') {
            if (tokenlist_push(out, TOK_ASSIGN, line, NULL) != 0) return -1;
            p += 2;
            continue;
        }

        /* Single-character tokens. */
        if (*p == ',') {
            if (tokenlist_push(out, TOK_COMMA, line, NULL) != 0) return -1;
            p++;
            continue;
        }
        if (*p == '(') {
            if (tokenlist_push(out, TOK_LPAREN, line, NULL) != 0) return -1;
            p++;
            continue;
        }
        if (*p == ')') {
            if (tokenlist_push(out, TOK_RPAREN, line, NULL) != 0) return -1;
            p++;
            continue;
        }
        if (*p == '[') {
            if (tokenlist_push(out, TOK_LBRACKET, line, NULL) != 0) return -1;
            p++;
            continue;
        }
        if (*p == ']') {
            if (tokenlist_push(out, TOK_RBRACKET, line, NULL) != 0) return -1;
            p++;
            continue;
        }

        /* Identifiers and keywords. */
        if (is_letter(*p) || *p == '_') {
            const char *start = p;
            const char *text;
            int kind;
            size_t len;
            p++;
            while (is_letter(*p) || is_digit(*p) || *p == '_') p++;
            len = (size_t)(p - start);
            text = intern_put(intern, start, len);
            if (!text) return -1;
            /* Check if it's a keyword. */
            {
                const char *kw = lex_keyword(start, len);
                if (kw) {
                    kind = (int)(size_t)kw;
                } else {
                    kind = TOK_IDENT;
                }
            }
            if (tokenlist_push(out, kind, line, text) != 0) return -1;
            continue;
        }

        /* Unknown character — skip with a warning? For now, fatal. */
        /* We let unknown chars through but they'll be parse errors. */
        p++;
    }

    if (tokenlist_push(out, TOK_EOF, line, NULL) != 0) return -1;
    return 0;
}

void lexer_free(TokenList *list)
{
    free(list->data);
    list->data = NULL;
    list->count = 0;
    list->capacity = 0;
}