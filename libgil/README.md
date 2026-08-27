# libgil

**libgil** is a compact, dependency-free C89 library for loading and running **Gil** — the *Gnosis Intent Language*. Gil is a three-valued logic language for defining *intents* that transform a *frontier* of predicate values.

![C89](https://img.shields.io/badge/C89-cross--platform-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-green)

- Load Gil scripts from a string or a file into an immutable `GilScript`.
- Execute intents against a frontier with iterative convergence and atomic per-iteration commits.
- Query the frontier with variable / literal pattern matching.
- Thread-safe when the caller provides a few lock callbacks.
- **Zero dependencies**, one self-documenting public header, strict C89.

```c
int main(void)
{
    const char *src =
        "intent lightOn()\n"
        "    lit <= true\n"
        "end\n";

    GilScript   *s = gil_load(src, NULL);
    GilIntent   *lightOn = gil_intent_get(s, "lightOn");
    GilFrontier *f = gil_frontier_new(NULL);

    gil_intent_execute(lightOn, f, NULL, 0);
    /* frontier now holds: lit = true */

    gil_frontier_free(f);
    gil_script_free(s);
    return 0;
}
```

---

## What is Gil?

Gil is a small language for describing *state transitions* over a **frontier** — a set of predicate/value bindings. A predicate is a proposition identified by a name and zero or more positional arguments:

```
alive
location[alice, room1]
owns[alice, sword]
```

Each predicate has one of three truth values:

- `true`
- `false` (the default for any predicate absent from the frontier)
- `both` (contradiction: simultaneously true and false)

An **intent** is a named, parameterized block of Gil statements. When executed, an intent evaluates all of its statements against the current frontier, collects the resulting **non-blocking assignments**, resolves conflicts (`true` + `false` → `both`), and commits them **atomically**. This repeats until no predicate value changes — the *convergence* fixed point.

Intents use synchronous, non-blocking assignment:

```
intent propagate(Node)
    activated[Node] <= true
    when activated[A] do
        when connected[A, B] do
            activated[B] <= true
        end
    end
end
```

The full Gil v1 specification lives in [`docs/gil.md`](docs/gil.md), including the three-valued truth tables and the execution semantics.

---

## Building

Requires a C89 compiler and `make`. No external dependencies.

```sh
make          # builds libgil.a
make test     # builds and runs the test suite (67 tests)
make clean    # removes build artifacts
```

To link against your own program:

```sh
cc myapp.c -Iinclude -L. -lgil -o myapp
```

---

## API overview

The public API lives in a single, fully self-documenting header: [`include/gil.h`](include/gil.h).

| Concern | Entry points |
|---------|---------------|
| Load scripts | `gil_load`, `gil_load_file`, `gil_script_free` |
| Intent access | `gil_intent_get` |
| Frontier | `gil_frontier_new`, `gil_frontier_set`, `gil_frontier_get`, `gil_frontier_del`, `gil_frontier_lock`, `gil_frontier_unlock`, `gil_frontier_free` |
| Query | `gil_frontier_query`, `gil_result_free` |
| Execute | `gil_intent_execute` |

### Example

```c
#include "gil.h"
#include <stdio.h>

int main(void)
{
    const char *err = NULL;
    GilScript *s;
    GilIntent *lightOn;
    GilFrontier *f;

    s = gil_load(
        "intent lightOn()\n"
        "    lit <= true\n"
        "end\n", &err);
    if (!s) { fprintf(stderr, "parse error: %s\n", err); return 1; }

    lightOn = gil_intent_get(s, "lightOn");
    f = gil_frontier_new(NULL);

    if (gil_intent_execute(lightOn, f, NULL, 0) != 0) {
        fprintf(stderr, "execution failed\n");
        return 1;
    }

    printf("lit = %d\n", (int)gil_frontier_get(f, "lit", NULL, 0)); /* 1 */

    gil_frontier_free(f);
    gil_script_free(s);
    return 0;
}
```

---

## Queries

Queries find every predicate in the frontier that matches a pattern. Pattern arguments use the Gil identifier convention: **UPPERCASE** names are *variables* (match any concrete value), **lowercase** names are *literals* (must match exactly).

```c
/* Match every owns[alice, X] in the frontier. */
const char *pat[] = { "alice", "Item" };
GilResult r = gil_frontier_query(f, "owns", pat, 2);
for (size_t i = 0; i < r.count; i++) {
    printf("%s owns %s = %d\n", r.matches[i].args[0],
           r.matches[i].args[1], (int)r.matches[i].value);
}
gil_result_free(&r);
```

A variable that repeats in a pattern (e.g. `linked[A, A]`) matches only predicates where both positions hold the same value. Query results borrow strings from the frontier; free the result container with `gil_result_free`. See [`include/gil.h`](include/gil.h) for full details.

---

## Threading model

`GilScript` and `GilIntent` are **immutable after creation** and safe to share across threads without any locking.

`GilFrontier` is mutable. For thread safety, pass a `GilLocks` callback set at creation time — the library never calls OS-specific threading primitives, so it works on any platform the caller does:

```c
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static void lock_cb(void *ctx) { pthread_mutex_lock((pthread_mutex_t *)ctx); }
static void unlock_cb(void *ctx) { pthread_mutex_unlock((pthread_mutex_t *)ctx); }
GilLocks locks = { &m, lock_cb, unlock_cb };
GilFrontier *f = gil_frontier_new(&locks);
```

When locks are provided, every `get`/`set`/`del`/`query` locks and unlocks automatically, and each convergence iteration commits atomically between iterations. Callers may batch many operations under one acquisition with `gil_frontier_lock` / `gil_frontier_unlock`. Pass `NULL` to `gil_frontier_new` for single-threaded use.

---

## Directory layout

```
libgil/
├── include/    # Public and internal headers (public API: gil.h)
├── src/        # Implementation (lexer, parser, frontier, query, executor)
├── test/       # Test suite
├── docs/       # Gil v1 language specification
├── Makefile
└── LICENSE     # MIT
```

## Testing

`make test` compiles and runs a 67-case suite covering: frontier basics, pattern queries, script loading (including malformed input), the full three-valued truth tables, operator precedence, parameterized intents, when-block guards, nested when-blocks, assignment conflict resolution, and the spec's `propagate_active` convergence example.

```sh
make test
# Results: 67/67 tests passed
```

## License

MIT — see [LICENSE](LICENSE).
