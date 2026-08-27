/* frontier.c -- predicate hash table with optional locking */

#include "frontier.h"
#include "gil.h"
#include <stdlib.h>
#include <string.h>

#define INIT_SIZE 64
#define LOAD_PCT  70

static size_t pred_hash(const char *name, const char *args[], size_t argc)
{
    size_t h = 2166136261u;
    size_t i, j;
    for (i = 0; name[i]; i++) {
        h ^= (unsigned char)name[i]; h *= 16777619u;
    }
    for (i = 0; i < argc; i++) {
        for (j = 0; args[i][j]; j++) {
            h ^= (unsigned char)args[i][j]; h *= 16777619u;
        }
    }
    return h;
}

static int pred_eq(const char *name, const char *args[], size_t argc,
                   const PredSlot *slot)
{
    size_t i;
    if (!slot->occupied) return 0;
    if (strcmp(slot->name, name) != 0) return 0;
    if (slot->argc != argc) return 0;
    for (i = 0; i < argc; i++)
        if (strcmp(slot->args[i], args[i]) != 0) return 0;
    return 1;
}

static int slot_copy_args(PredSlot *slot, const char *args[], size_t argc)
{
    size_t i;
    if (argc == 0) { slot->args = NULL; return 0; }
    slot->args = (char**)malloc(argc * sizeof(char*));
    if (!slot->args) return -1;
    for (i = 0; i < argc; i++) {
        slot->args[i] = (char*)malloc(strlen(args[i]) + 1);
        if (!slot->args[i]) {
            size_t j;
            for (j = 0; j < i; j++) free(slot->args[j]);
            free(slot->args); slot->args = NULL; return -1;
        }
        strcpy(slot->args[i], args[i]);
    }
    return 0;
}

static void slot_free_pred(PredSlot *slot)
{
    size_t i;
    if (slot->name) { free((void*)slot->name); slot->name = NULL; }
    if (slot->args) {
        for (i = 0; i < slot->argc; i++) free(slot->args[i]);
        free(slot->args); slot->args = NULL;
    }
}

static int frontier_resize(GilFrontier *f)
{
    size_t new_cap = f->capacity * 2;
    PredSlot *old = f->slots, *new_slots;
    size_t old_cap = f->capacity, i;
    new_slots = (PredSlot*)calloc(new_cap, sizeof(PredSlot));
    if (!new_slots) return -1;
    f->slots = new_slots; f->capacity = new_cap; f->count = 0;
    for (i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            size_t idx = pred_hash(old[i].name,
                (const char**)old[i].args, old[i].argc) & (new_cap-1);
            while (f->slots[idx].occupied)
                idx = (idx + 1) & (new_cap - 1);
            f->slots[idx] = old[i]; f->count++;
        }
    }
    free(old);
    return 0;
}

static void frontier_lock(GilFrontier *f)
{
    if (f->has_locks && f->lock_depth == 0)
        f->locks_copy.lock(f->locks_copy.ctx);
    f->lock_depth++;
}
static void frontier_unlock(GilFrontier *f)
{
    f->lock_depth--;
    if (f->has_locks && f->lock_depth == 0)
        f->locks_copy.unlock(f->locks_copy.ctx);
}
/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

GilFrontier* gil_frontier_new(GilLocks *locks)
{
    GilFrontier *f = (GilFrontier*)calloc(1, sizeof(GilFrontier));
    if (!f) return NULL;
    f->slots = (PredSlot*)calloc(INIT_SIZE, sizeof(PredSlot));
    if (!f->slots) { free(f); return NULL; }
    f->capacity = INIT_SIZE;
    if (locks) { f->locks_copy = *locks; f->has_locks = 1; }
    return f;
}

void gil_frontier_set(GilFrontier *f, const char *name,
                      const char *args[], size_t argc, GilVal value)
{
    size_t idx;
    if (!f || !name) return;
    frontier_lock(f);
    idx = pred_hash(name, args, argc) & (f->capacity - 1);
    while (f->slots[idx].occupied) {
        if (pred_eq(name, args, argc, &f->slots[idx])) {
            f->slots[idx].value = value;
            frontier_unlock(f);
            return;
        }
        idx = (idx + 1) & (f->capacity - 1);
    }
    if (f->count * 100 >= f->capacity * LOAD_PCT) {
        if (frontier_resize(f) != 0) { frontier_unlock(f); return; }
        idx = pred_hash(name, args, argc) & (f->capacity - 1);
        while (f->slots[idx].occupied)
            idx = (idx + 1) & (f->capacity - 1);
    }
    f->slots[idx].name = (char*)malloc(strlen(name) + 1);
    if (!f->slots[idx].name) { frontier_unlock(f); return; }
    strcpy((char*)f->slots[idx].name, name);
    if (slot_copy_args(&f->slots[idx], args, argc) != 0) {
        free((void*)f->slots[idx].name);
        f->slots[idx].name = NULL;
        frontier_unlock(f); return;
    }
    f->slots[idx].argc = argc;
    f->slots[idx].value = value;
    f->slots[idx].occupied = 1;
    f->count++;
    frontier_unlock(f);
}

GilVal gil_frontier_get(const GilFrontier *f, const char *name,
                        const char *args[], size_t argc)
{
    size_t idx;
    GilVal v;
    if (!f || !name) return GIL_FALSE;
    frontier_lock((GilFrontier*)f);
    idx = pred_hash(name, args, argc) & (f->capacity - 1);
    while (f->slots[idx].occupied) {
        if (pred_eq(name, args, argc, &f->slots[idx])) {
            v = f->slots[idx].value;
            frontier_unlock((GilFrontier*)f);
            return v;
        }
        idx = (idx + 1) & (f->capacity - 1);
    }
    frontier_unlock((GilFrontier*)f);
    return GIL_FALSE;
}

void gil_frontier_del(GilFrontier *f, const char *name,
                      const char *args[], size_t argc)
{
    size_t idx;
    if (!f || !name) return;
    frontier_lock(f);
    idx = pred_hash(name, args, argc) & (f->capacity - 1);
    while (f->slots[idx].occupied) {
        if (pred_eq(name, args, argc, &f->slots[idx])) {
            slot_free_pred(&f->slots[idx]);
            f->slots[idx].occupied = 0;
            f->count--;
            frontier_unlock(f);
            return;
        }
        idx = (idx + 1) & (f->capacity - 1);
    }
    frontier_unlock(f);
}

void gil_frontier_lock(GilFrontier *f)
{
    if (f) frontier_lock(f);
}

void gil_frontier_unlock(GilFrontier *f)
{
    if (f) frontier_unlock(f);
}

void gil_frontier_free(GilFrontier *f)
{
    size_t i;
    if (!f) return;
    if (f->slots) {
        for (i = 0; i < f->capacity; i++)
            if (f->slots[i].occupied) slot_free_pred(&f->slots[i]);
        free(f->slots);
    }
    free(f);
}
