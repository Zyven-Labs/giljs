/* intern.h -- string interning table (internal)
 *
 * The interning table owns all string copies. Once a string is interned,
 * pointer equality is string equality. Strings are never deleted; the
 * entire table is freed at script unload time.
 */

#ifndef GIL_INTERN_H
#define GIL_INTERN_H

#include <stddef.h> /* size_t */

typedef struct Intern Intern;

/* Create a new empty interning table. */
Intern* intern_new(void);

/* Intern a string of known length. Returns a pointer valid until the
   Intern is destroyed. If the string already exists, returns the
   existing pointer. The string is copied; the caller retains ownership
   of the original. An explicit length allows embedded NULs (not relevant
   for Gil but keeps the interface general). */
const char* intern_put(Intern *t, const char *s, size_t len);

/* Same as intern_put when s is null-terminated. */
const char* intern_str(Intern *t, const char *s);

/* Destroy the table and all owned strings. */
void intern_free(Intern *t);

#endif /* GIL_INTERN_H */