/* gil.c -- libgil top-level public API implementation */

#include "gil.h"
#include "load.h"
#include "script.h"
#include "parser.h"
#include "intern.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* gil_load                                                           */
/* ------------------------------------------------------------------ */

GilScript* gil_load(const char *source, const char **error)
{
    return load_from_source(source, error);
}

GilScript* gil_load_file(const char *path, const char **error)
{
    return load_from_file(path, error);
}

/* ------------------------------------------------------------------ */
/* gil_script_free                                                    */
/* ------------------------------------------------------------------ */

void gil_script_free(GilScript *script)
{
    if (!script) return;
    if (script->intents) {
        parser_free_intents(script->intents, script->intent_count);
        script->intents = NULL;
    }
    if (script->intern) { intern_free(script->intern); script->intern = NULL; }
    free(script);
}

/* ------------------------------------------------------------------ */
/* gil_intent_get                                                     */
/* ------------------------------------------------------------------ */

GilIntent* gil_intent_get(const GilScript *script, const char *name)
{
    int i;
    if (!script || !name) return NULL;
    for (i = 0; i < script->intent_count; i++) {
        if (script->intents[i].name == name)
            return (GilIntent*)&script->intents[i];
    }
    /* Also try strcmp in case name came from outside the intern table. */
    for (i = 0; i < script->intent_count; i++) {
        if (strcmp(script->intents[i].name, name) == 0)
            return (GilIntent*)&script->intents[i];
    }
    return NULL;
}