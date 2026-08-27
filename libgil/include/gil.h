/* gil.h -- libgil: load, execute, and query Gil (Gnosis Intent Language) scripts
 *
 * Gil is a three-valued logic language for defining intents that transform
 * a Gnosis frontier. A frontier is a set of predicate/value pairs. Predicates
 * are propositions identified by a name and zero or more positional arguments.
 * Predicate values are one of: true, false, or both. A predicate not present
 * in the frontier has value false. The value `both` represents contradiction:
 * the predicate is both true and false.
 *
 * A Gil program defines intents. An intent is a named transition that evaluates
 * against the current frontier and produces a new frontier. Intents use
 * synchronous non-blocking assignment semantics (<=) — assignments are
 * collected during evaluation and committed atomically at the end of each
 * convergence iteration.
 *
 * Variables and literals are distinguished by identifier case:
 *   - Identifiers beginning with an uppercase letter are VARIABLES.
 *   - Identifiers beginning with a lowercase letter are LITERAL CONSTANTS.
 *
 * Variables are bound by intent parameters or by pattern matching in when
 * clauses. An unbound variable error is reported at script load time.
 *
 * --- Intent execution model ---
 * 1. Bind intent parameters from invocation arguments.
 * 2. Repeat until no assignments are produced (stable state):
 *    a. Evaluate all statements against the current frontier; the frontier
 *       is NOT modified during evaluation.
 *    b. Collect all generated assignments.
 *    c. Resolve conflicting assignments (true+false -> both, etc.).
 *    d. Commit resolved assignments atomically into the frontier.
 *    e. If any predicate value changed, repeat from step 2.
 * 3. Terminate when no assignments are produced (convergence reached).
 *
 * --- Expression truth tables ---
 * NOT            AND                  OR
 *   A     not A    A B   A and B      A B   A or B
 *   true  false    T T   true         T T   true
 *   false true     T F   false        T F   true
 *   both  both     T 2   both         T 2   true
 *                  F T   false        F T   true
 *                  F F   false        F F   false
 *                  F 2   false        F 2   both
 *                  2 T   both        2 T   true
 *                  2 F   false       2 F   both
 *                  2 2   both        2 2   both
 *
 * Operator precedence: not > and > or. Parentheses override precedence.
 *
 * --- Integer constants and arithmetic ---
 * Predicate arguments may be identifiers (variables/literals) or integer
 * constants. Within an argument position, arithmetic operators (+ - * /)
 * combine integer constants and variables bound to integer constants;
 * * and / bind tighter than + and -. Constant-only arithmetic is folded
 * to a single integer at load time; arithmetic referencing variables is
 * evaluated at execution time. Integers are never assigned with <= and
 * are never converted to a logical value: they exist only as constants
 * that can be matched against the frontier. Division by a constant zero
 * is a load-time error; division by a variable that evaluates to zero, or
 * using a non-integer variable operand, is a runtime error (the frontier
 * keeps its last committed state).
 *
 * --- Thread safety ---
 * GilScript and GilIntent are immutable after creation and may be shared
 * across threads without locking. Each GilFrontier carries an optional
 * mutex provided by the caller at creation time (via GilLocks). Pass NULL
 * to GilFrontier locks for single-threaded use. Internal operations
 * (get/set/del/query) lock and unlock automatically. Callers may batch
 * multiple operations under one lock acquisition using
 * gil_frontier_lock() / gil_frontier_unlock().
 *
 * --- C89 compliance ---
 * This library is written in maximally cross-platform C89. No inline
 * functions, no anonymous structs, no mixed declarations. All public
 * symbols are prefixed with gil_ or Gil.
 */

#ifndef GIL_H
#define GIL_H

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------------------------------------------------------------------
 * Three-valued logic
 * -------------------------------------------------------------------------*/

typedef enum {
    GIL_FALSE = 0,
    GIL_TRUE  = 1,
    GIL_BOTH  = 2
} GilVal;


/* ---------------------------------------------------------------------------
 * Opaque handles
 * -------------------------------------------------------------------------*/

typedef struct GilScript   GilScript;
typedef struct GilFrontier GilFrontier;
typedef struct GilIntent   GilIntent;


/* ---------------------------------------------------------------------------
 * Caller-provided locking callbacks
 * -------------------------------------------------------------------------*/

typedef struct GilLocks {
    void *ctx;
    void (*lock)(void *ctx);
    void (*unlock)(void *ctx);
} GilLocks;
/* ---------------------------------------------------------------------------
 * Script loading
 * -------------------------------------------------------------------------*/

/* gil_load parses a Gil program from a null-terminated source string.
 * The source string may be freed after this call returns; the script
 * interns all needed identifier strings internally.
 *
 * Returns a pointer to a new GilScript on success.
 * Returns NULL on parse error and sets *error to point to a
 * descriptive, statically-allocated error string.
 *
 * Example:
 *   GilScript *s = gil_load("intent hello() end", &err);
 *   if (!s) { fprintf(stderr, "error: %s\n", err); return; } */
GilScript* gil_load(const char *source, const char **error);


/* gil_load_file reads a file, parses it as a Gil program, and returns
 * a new GilScript. Internally reads the entire file, parses, and frees
 * the buffer. The path must point to a readable regular file.
 *
 * Returns NULL on I/O or parse error and sets *error to a descriptive
 * string. The error string is statically allocated and may describe
 * either a file-system issue or a parse error (with line number).
 *
 * Example:
 *   GilScript *s = gil_load_file("game.gil", &err);
 *   if (!s) { fprintf(stderr, "error: %s\n", err); return; } */
GilScript* gil_load_file(const char *path, const char **error);


/* gil_script_free releases all memory owned by a GilScript, including
 * all parsed intents. Passing NULL is safe and is a no-op. */
void gil_script_free(GilScript *script);


/* ---------------------------------------------------------------------------
 * Intent access
 * -------------------------------------------------------------------------*/

/* gil_intent_get looks up an intent by name in a loaded script.
 * The returned pointer is valid until the script is freed.
 * Returns NULL if no intent with the given name exists.
 *
 * Intent names are case-sensitive and must match the declaration name
 * from the Gil source exactly.
 *
 * Example:
 *   GilIntent *move = gil_intent_get(script, "move");
 *   if (!move) { fprintf(stderr, "intent not found\n"); return; } */
GilIntent* gil_intent_get(const GilScript *script, const char *name);


/* ---------------------------------------------------------------------------
 * Frontier management
 * -------------------------------------------------------------------------*/

/* gil_frontier_new creates an empty frontier. All predicates are
 * implicitly GIL_FALSE. Pass locks as NULL for single-threaded use,
 * or a pointer to a GilLocks struct to enable thread safety.
 *
 * The GilLocks struct may be stack-allocated; the frontier copies
 * the pointer values internally. lock and unlock may be called many
 * times per operation so they should be lightweight.
 *
 * Example (single-threaded):
 *   GilFrontier *f = gil_frontier_new(NULL);
 *
 * Example (with pthreads):
 *   static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
 *   static void lk(void *c) { pthread_mutex_lock((pthread_mutex_t*)c); }
 *   static void ul(void *c) { pthread_mutex_unlock((pthread_mutex_t*)c); }
 *   GilLocks locks = { &m, lk, ul };
 *   GilFrontier *f = gil_frontier_new(&locks); */
GilFrontier* gil_frontier_new(GilLocks *locks);
/* gil_frontier_set assigns a value to a predicate in the frontier.
 * If the predicate already exists, its value is overwritten.
 * If the predicate is new, it is inserted.
 *
 * name is the predicate name (case-sensitive). args is an array of
 * argc string pointers representing the predicate's positional
 * arguments. Pass NULL and argc=0 for predicates with no arguments.
 *
 * All string pointers are borrowed during the call only; the
 * frontier interns or copies them internally as needed.
 *
 * This operation locks the frontier when GilLocks was provided. */
void gil_frontier_set(GilFrontier *f, const char *name,
                      const char *args[], size_t argc, GilVal value);


/* gil_frontier_get retrieves the value of a predicate in the frontier.
 * If the predicate is not present in the frontier, returns GIL_FALSE
 * (the default value per spec).
 *
 * All string pointers are borrowed during the call.
 * This operation locks the frontier when GilLocks was provided. */
GilVal gil_frontier_get(const GilFrontier *f, const char *name,
                        const char *args[], size_t argc);


/* gil_frontier_del removes a predicate from the frontier. Subsequent
 * calls to gil_frontier_get for this predicate will return GIL_FALSE.
 * If the predicate is not present, the call is a no-op.
 *
 * All string pointers are borrowed during the call.
 * This operation locks the frontier when GilLocks was provided. */
void gil_frontier_del(GilFrontier *f, const char *name,
                      const char *args[], size_t argc);


/* gil_frontier_lock acquires the frontier's mutex (if any). Allows
 * callers to batch multiple operations under a single lock acquisition
 * to avoid per-call lock/unlock overhead. Must be paired with
 * gil_frontier_unlock.
 *
 * If no GilLocks was provided, this is a no-op. */
void gil_frontier_lock(GilFrontier *f);


/* gil_frontier_unlock releases the frontier's mutex previously
 * acquired by gil_frontier_lock. If no GilLocks was provided, this
 * is a no-op. */
void gil_frontier_unlock(GilFrontier *f);


/* gil_frontier_free releases all memory owned by the frontier.
 * Passing NULL is safe and is a no-op. */
void gil_frontier_free(GilFrontier *f);
/* ---------------------------------------------------------------------------
 * Querying
 * -------------------------------------------------------------------------*/

/* GilMatch represents a single matching predicate from a query result.
 * All string pointers point into the frontier's internal storage and
 * are valid until the frontier is freed (or the predicate is deleted).
 * Do not free or modify these strings. */
typedef struct GilMatch {
    const char  *name;
    const char **args;
    size_t       argc;
    GilVal       value;
} GilMatch;


/* GilResult contains the set of matches returned by a query.
 * The matches array is heap-allocated and must be freed with
 * gil_result_free(). */
typedef struct GilResult {
    GilMatch *matches;
    size_t    count;
} GilResult;


/* gil_frontier_query finds all ground predicates in the frontier
 * that match the given pattern. The pattern uses the Gil identifier
 * convention to distinguish variables from literals:
 *
 *   UPPERCASE arguments are VARIABLES and match any concrete value.
 *   lowercase arguments are LITERALS and must match exactly.
 *   The pattern name itself is always matched literally.
 *
 * A variable that appears multiple times in the same pattern
 * (e.g. connected[A, A]) matches only predicates where both
 * positions have the same concrete value.
 *
 * Example:
 *   GilResult r = gil_frontier_query(f, "owns",
 *       (const char*[]){"Alice", "?"}, 2);
 *   // matches owns[Alice, x] for every x in the frontier
 *
 *   GilResult r = gil_frontier_query(f, "connected",
 *       (const char*[]){"RoomA", "RoomB"}, 2);
 *   // matches exactly connected[RoomA, RoomB]
 *
 * The returned GilResult must be freed with gil_result_free().
 * This operation locks the frontier when GilLocks was provided. */
GilResult gil_frontier_query(const GilFrontier *f, const char *name,
                             const char *pattern_args[], size_t argc);


/* gil_result_free frees the matches array within a GilResult. It does
 * NOT free the string pointers inside each GilMatch (those point into
 * the frontier's internal storage). Passing a result with count==0 or
 * matches==NULL is safe and is a no-op. */
void gil_result_free(GilResult *r);


/* ---------------------------------------------------------------------------
 * Intent execution
 * -------------------------------------------------------------------------*/

/* gil_intent_execute runs a named intent against the frontier in-place.
 *
 * intent   -- the intent to execute (from gil_intent_get).
 * frontier -- the frontier to evaluate against and modify.
 * args     -- array of argc concrete argument strings bound to the
 *             intent's parameters in declaration order. Pointers are
 *             borrowed during the call.
 * argc     -- number of arguments. Must match the intent's declared
 *             parameter count exactly.
 *
 * Execution follows the Gil convergence loop:
 *   1. Bind intent parameters from args.
 *   2. Repeat until no assignments are produced:
 *      a. Evaluate all statements against the current frontier,
 *         accumulating assignments.
 *      b. Resolve conflicts (true+false -> both, etc.).
 *      c. Lock frontier, commit resolved assignments, unlock.
 *      d. If any predicate value changed, repeat.
 *   3. Return 0 (stable state reached).
 *
 * Between iterations, the frontier is in a committed, consistent
 * state. Other threads may read or set predicates on the same
 * frontier during those intervals.
 *
 * Returns 0 on successful convergence.
 * Returns -1 on error (e.g. argument count mismatch or unbound
 * variable encountered during execution). In case of error the
 * frontier state is the last committed state; partial iterations
 * are not committed.
 *
 * Example:
 *   GilIntent *propagate = gil_intent_get(script, "propagate_active");
 *   const char *args[] = { "start" };
 *   if (gil_intent_execute(propagate, frontier, args, 1) != 0)
 *       fprintf(stderr, "execution failed\n"); */
int gil_intent_execute(GilIntent *intent, GilFrontier *frontier,
                       const char *args[], size_t argc);


#ifdef __cplusplus
}
#endif

#endif /* GIL_H */