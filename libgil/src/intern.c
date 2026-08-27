/* intern.c -- string interning table implementation */

#include "intern.h"
#include <stdlib.h>
#include <string.h>

/* Initial hash table capacity. Must be a power of two. */
#define INTERN_INIT_SIZE 64

/* Load factor threshold before resizing (percent / 100). */
#define INTERN_LOAD_FACTOR 70

typedef struct {
    char *str;       /* owned string copy, or NULL for an empty slot */
    size_t len;      /* length of str */
    unsigned hash;   /* cached hash value */
} InternSlot;

struct Intern {
    InternSlot *slots;
    size_t       capacity;
    size_t       count;
};

/* FNV-1a hash. Simple, fast, and works well for identifier strings. */
static unsigned hash_str(const char *s, size_t len)
{
    unsigned h = 2166136261u;
    size_t i;
    for (i = 0; i < len; i++) {
        h ^= (unsigned char)s[i];
        h *= 16777619u;
    }
    return h;
}

Intern* intern_new(void)
{
    Intern *t;
    t = (Intern*)malloc(sizeof(Intern));
    if (!t) return NULL;
    t->slots = (InternSlot*)calloc(INTERN_INIT_SIZE, sizeof(InternSlot));
    if (!t->slots) {
        free(t);
        return NULL;
    }
    t->capacity = INTERN_INIT_SIZE;
    t->count = 0;
    return t;
}

static int intern_resize(Intern *t)
{
    size_t new_cap = t->capacity * 2;
    InternSlot *old_slots = t->slots;
    size_t old_cap = t->capacity;
    InternSlot *new_slots;
    size_t i;

    new_slots = (InternSlot*)calloc(new_cap, sizeof(InternSlot));
    if (!new_slots) return -1;

    t->slots = new_slots;
    t->capacity = new_cap;
    t->count = 0;

    for (i = 0; i < old_cap; i++) {
        if (old_slots[i].str) {
            size_t idx = old_slots[i].hash & (new_cap - 1);
            while (new_slots[idx].str) {
                idx = (idx + 1) & (new_cap - 1);
            }
            new_slots[idx].str = old_slots[i].str;
            new_slots[idx].len = old_slots[i].len;
            new_slots[idx].hash = old_slots[i].hash;
            t->count++;
        }
    }
    free(old_slots);
    return 0;
}

const char* intern_put(Intern *t, const char *s, size_t len)
{
    unsigned h;
    size_t idx;
    InternSlot *slot;

    if (!t || !s) return NULL;

    h = hash_str(s, len);
    idx = h & (t->capacity - 1);

    for (;;) {
        slot = &t->slots[idx];
        if (!slot->str) break;
        if (slot->hash == h && slot->len == len &&
            memcmp(slot->str, s, len) == 0) {
            return slot->str;
        }
        idx = (idx + 1) & (t->capacity - 1);
    }

    /* Not found — insert if there's room (after possible resize). */
    if (t->count * 100 >= t->capacity * INTERN_LOAD_FACTOR) {
        if (intern_resize(t) != 0) return NULL;
        /* Re-probe after resize. */
        idx = h & (t->capacity - 1);
        for (;;) {
            slot = &t->slots[idx];
            if (!slot->str) break;
            idx = (idx + 1) & (t->capacity - 1);
        }
    }

    slot->str = (char*)malloc(len + 1);
    if (!slot->str) return NULL;
    memcpy(slot->str, s, len);
    slot->str[len] = '\0';
    slot->len = len;
    slot->hash = h;
    t->count++;
    return slot->str;
}

const char* intern_str(Intern *t, const char *s)
{
    if (!s) return NULL;
    return intern_put(t, s, strlen(s));
}

void intern_free(Intern *t)
{
    size_t i;
    if (!t) return;
    if (t->slots) {
        for (i = 0; i < t->capacity; i++) {
            free(t->slots[i].str);
        }
        free(t->slots);
    }
    free(t);
}