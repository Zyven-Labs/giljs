/* query.c -- gil_frontier_query implementation */

#include "gil.h"
#include "frontier.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check if a pattern element matches a concrete value.
   pattern_arg is uppercase = variable (any), lowercase = literal exact. */
static int pat_match(const char *pattern_arg, const char *concrete)
{
    if (!pattern_arg || !concrete) return 0;
    if (isupper((unsigned char)pattern_arg[0])) return 1;
    return strcmp(pattern_arg, concrete) == 0;
}

GilResult gil_frontier_query(const GilFrontier *f, const char *name,
                             const char *pattern_args[], size_t argc)
{
    GilResult result;
    size_t    i;
    size_t    cap = 16;

    result.matches = NULL;
    result.count = 0;
    if (!f || !name) return result;

    gil_frontier_lock((GilFrontier*)f);

    result.matches = (GilMatch*)malloc(cap * sizeof(GilMatch));
    if (!result.matches) {
        gil_frontier_unlock((GilFrontier*)f);
        return result;
    }

    for (i = 0; i < f->capacity; i++) {
        PredSlot *slot = &f->slots[i];
        if (!slot->occupied) continue;
        if (strcmp(slot->name, name) != 0) continue;
        if (slot->argc != argc) continue;

        /* Check all pattern args match slot args. */
        {
            size_t j;
            int match = 1;
            for (j = 0; j < argc; j++) {
                if (!pat_match(pattern_args[j], slot->args[j])) {
                    match = 0;
                    break;
                }
            }
            if (!match) continue;
        }

        /* Expand result array if needed. */
        if (result.count >= cap) {
            cap *= 2;
            {
                GilMatch *tmp = (GilMatch*)realloc(result.matches,
                    cap * sizeof(GilMatch));
                if (!tmp) {
                    gil_result_free(&result);
                    gil_frontier_unlock((GilFrontier*)f);
                    return result;
                }
                result.matches = tmp;
            }
        }

        result.matches[result.count].name  = slot->name;
        result.matches[result.count].args  = (const char**)slot->args;
        result.matches[result.count].argc  = slot->argc;
        result.matches[result.count].value = slot->value;
        result.count++;
    }

    gil_frontier_unlock((GilFrontier*)f);
    return result;
}

void gil_result_free(GilResult *r)
{
    if (r && r->matches) {
        free(r->matches);
        r->matches = NULL;
        r->count = 0;
    }
}