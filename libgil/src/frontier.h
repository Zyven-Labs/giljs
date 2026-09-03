/* frontier.h -- predicate storage (internal) */

#ifndef GIL_FRONTIER_H
#define GIL_FRONTIER_H

#include "gil.h"

/* Internal structure; public API is in gil.h */

/* A single predicate entry in the frontier hash table.
   Strings are deep-copied and owned by the slot. */
typedef struct {
    const char *name;       /* predicate name; owned, deep-copied          */
    char      **args;       /* argument array copies; OWNED          */
    size_t      argc;
    GilVal      value;
    int         occupied;   /* 0 = empty slot                        */
} PredSlot;

struct GilFrontier {
    PredSlot  *slots;
    size_t     capacity;
    size_t     count;
    GilLocks   locks_copy;  /* copy of the GilLocks struct           */
    int        has_locks;   /* 1 if locks were provided              */
    int        lock_depth;  /* reentrant lock counter                */
};

#endif /* GIL_FRONTIER_H */