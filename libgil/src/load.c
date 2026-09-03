/* load.c -- gil_load / gil_load_file implementation */

#include "load.h"
#include "lexer.h"
#include "parser.h"
#include "intern.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

GilScript* load_from_source(const char *source, const char **error)
{
    Intern    *intern;
    TokenList  tokens;
    GilScript *script;
    int        count;

    intern = intern_new();
    if (!intern) {
        if (error) *error = "out of memory";
        return NULL;
    }

    if (lexer_tokenize(source, intern, &tokens, error) != 0) {
        intern_free(intern);
        if (error) *error = "out of memory";
        return NULL;
    }

    script = (GilScript*)malloc(sizeof(GilScript));
    if (!script) {
        lexer_free(&tokens);
        intern_free(intern);
        if (error) *error = "out of memory";
        return NULL;
    }

    script->intern = intern;
    script->intents = parser_parse(&tokens, intern, &count, error);
    script->intent_count = count;

    lexer_free(&tokens);

    if (!script->intents) {
        /* Parse error — error already set by parser */
        intern_free(intern);
        free(script);
        return NULL;
    }

    return script;
}

GilScript* load_from_file(const char *path, const char **error)
{
    FILE *fp;
    long  size;
    char *buf;
    GilScript *script;

    fp = fopen(path, "rb");
    if (!fp) {
        if (error) *error = "cannot open file";
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) {
        fclose(fp);
        if (error) *error = "cannot read file";
        return NULL;
    }

    buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        if (error) *error = "out of memory";
        return NULL;
    }

    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        if (error) *error = "cannot read file";
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);

    script = load_from_source(buf, error);
    free(buf);
    return script;
}