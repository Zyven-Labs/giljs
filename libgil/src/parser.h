/* parser.h -- Gil recursive-descent parser (internal) */

#ifndef GIL_PARSER_H
#define GIL_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "intern.h"

/* Parse a token list into a GilScript representation.
   On success, returns a dynamically-allocated array of Intent structs
   and sets *intent_count. On error, returns NULL and sets *error to a
   static string describing the problem (includes source line number).
   Caller must free the returned array and all its memory.           */
Intent* parser_parse(TokenList *tokens, Intern *intern,
                     int *intent_count, const char **error);

/* Free all memory owned by a parser-produced Intent array.
   Does NOT free the intern table (that's owned by the caller). */
void parser_free_intents(Intent *intents, int count);

#endif /* GIL_PARSER_H */