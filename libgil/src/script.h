/* script.h -- GilScript internal definition (internal)
 *
 * GilScript owns all parsed data: interning table, intent array.
 * It is immutable after creation.
 */

#ifndef GIL_SCRIPT_H
#define GIL_SCRIPT_H

#include "gil.h"
#include "ast.h"
#include "intern.h"

struct GilScript {
    Intern  *intern;       /* string table                       */
    int      intent_count;
    Intent  *intents;      /* array of parsed intents            */
};

#endif /* GIL_SCRIPT_H */