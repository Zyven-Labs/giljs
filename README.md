# giljs

**Node.js native addon for [libgil](libgil/) — the Gnosis Intent Language runtime.**

```js
const { Script, Frontier, Intent, GIL } = require('@zyvenlabs/giljs');

const src = `intent lightOn()
    lit <= true
end`;

const s = Script.load(src);
const f = new Frontier();
const lightOn = s.intent('lightOn');

lightOn.execute(f);

console.log(f.get('lit')); // GIL.TRUE (1)
```

---

## What is Gil?

Gil is a small three-valued logic language for describing *intents* — named,
parameterized transitions that read and write predicates on a **frontier**.
The language itself is documented in [`libgil/README.md`](libgil/) and
specified in [`libgil/docs/gil.md`](libgil/docs/gil.md).

This package lets you load Gil scripts, create frontiers, and execute intents
from Node.js — all through a clean, garbage-collected C++ native addon.

---

## Classes

### `Frontier`

A frontier is a mutable set of predicate/value bindings. Every predicate
absent from the frontier has value `GIL.FALSE` by default.

| Method | Returns | Description |
|--------|---------|-------------|
| `get(name, args?)` | `GIL.FALSE` (value `0`), `GIL.TRUE` (value `1`), or `GIL.BOTH` (value `2`) | Look up a predicate value |
| `set(name, args?, value?)` | `undefined` | Set a predicate's value (defaults to `GIL.TRUE`) |
| `del(name, args?)` | `undefined` | Remove a predicate from the frontier |
| `query(name, pattern?)` | `{ matches: [...] }` | Pattern-match predicates (see [Queries](#queries)) |
| `lock()` | `undefined` | Acquire the frontier's internal lock |
| `unlock()` | `undefined` | Release the internal lock |

**Predicates** are identified by a name and zero or more positional string arguments:

```js
const f = new Frontier();

// No-argument predicate
f.set('alive', GIL.TRUE);
f.get('alive');        // GIL.TRUE

// Predicate with arguments
f.set('owns', ['alice', 'sword'], GIL.TRUE);
f.get('owns', ['alice', 'sword']);  // GIL.TRUE

// Absent predicates are false
f.get('nonexistent');  // GIL.FALSE

// Overwrite a value
f.set('owns', ['alice', 'sword'], GIL.BOTH);
f.get('owns', ['alice', 'sword']);  // GIL.BOTH

// Delete
f.del('owns', ['alice', 'sword']);
f.get('owns', ['alice', 'sword']);  // GIL.FALSE
```

#### Queries

Queries find every predicate matching a pattern. Pattern arguments use Gil's
identifier convention:

- **UPPERCASE** names are *variables* — they match any concrete value.
- **lowercase** names are *literals* — they must match exactly.

A variable that repeats (e.g. `linked[A, A]`) matches only predicates where
both positions hold the same value.

```js
f.set('owns', ['alice', 'sword'],  GIL.TRUE);
f.set('owns', ['alice', 'shield'], GIL.TRUE);
f.set('owns', ['bob',   'sword'],  GIL.TRUE);

// Find everything alice owns
const r = f.query('owns', ['alice', 'Item']);
// r.matches = [
//   { args: ['alice', 'sword'],  value: 1 },
//   { args: ['alice', 'shield'], value: 1 }
// ]

// Find everyone who owns a sword
const r2 = f.query('owns', ['Who', 'sword']);
```

---

### `Script`

A compiled Gil script — immutable after creation. It is automatically freed
when the JavaScript object is garbage collected.

| Static Method | Returns | Description |
|---------------|---------|-------------|
| `Script.load(source)` | `Script` | Compile a Gil source string |
| `Script.loadFile(path)` | `Script` | Compile a Gil source file |

| Instance Method | Returns | Description |
|-----------------|---------|-------------|
| `Script.intent(name)` | `Intent` \| `undefined` | Look up a named intent |

```js
// From a string
const s = Script.load(`intent greet(Name)
    greeted[Name] <= true
end`);

// From a file
const s2 = Script.loadFile('./scripts/mygame.gil');

// Get an intent
const greet = s.intent('greet');   // Intent object
const nope  = s.intent('absent');  // undefined — not found
```

On syntax errors both methods throw an `Error`:

```js
try {
    Script.load('intent broken');
} catch (e) {
    // e.message describes the parse error
}
```

---

### `Intent`

A named, executable intent from a loaded script. Intents do not own their
own memory — the parent `Script` is kept alive automatically as long as any
`Intent` object derived from it exists.

| Method | Description |
|--------|-------------|
| `intent.execute(frontier, args?)` | Execute the intent against the frontier |

```js
const s = Script.load(`intent lightOn()
    lit <= true
end`);
const f = new Frontier();
const intent = s.intent('lightOn');

intent.execute(f);
// f now contains: lit = true
```

#### Parameterized intents

Intents declare parameters as uppercase names. Pass concrete argument values
as an array:

```js
const s = Script.load(`intent setAt(Pred, Loc)
    Pred[Loc] <= true
end`);
const intent = s.intent('setAt');

intent.execute(f, ['active', 'room1']);
// f now contains: active[room1] = true
```

If the wrong number of arguments is provided, `execute()` throws an `Error`.

#### Convergence

Execution follows Gil's convergence loop: statements are evaluated against
the frontier, non-blocking assignments are collected and resolved, then
committed atomically. This repeats until no predicate value changes (a
fixed point). The `propagate_active` example from the spec illustrates this:

```js
const src = `intent propagate_active(Node)
    activated[Node] <= true
    when activated[A] do
        when connected[A, B] do
            activated[B] <= true
        end
    end
end`;

const s = Script.load(src);
const f = new Frontier();
f.set('connected', ['a', 'b'], GIL.TRUE);
f.set('connected', ['b', 'c'], GIL.TRUE);

s.intent('propagate_active').execute(f, ['a']);

f.get('activated', ['a']); // GIL.TRUE
f.get('activated', ['b']); // GIL.TRUE
f.get('activated', ['c']); // GIL.TRUE
```

---

## Constants: `GIL`

```js
const { GIL } = require('@zyvenlabs/giljs');

GIL.FALSE  // 0
GIL.TRUE   // 1
GIL.BOTH   // 2
```

These are the three truth values used by `Frontier.get()` and `Frontier.set()`.

---

## Error Handling

All type errors (wrong argument types, missing arguments) throw a standard
`TypeError`. Script parsing and intent execution failures throw `Error`.
Intent argument count mismatches throw `Error`. There are no unchecked
exceptions.

---

## Installation

```sh
npm install @zyvenlabs/giljs
```

The addon is compiled from C/C++ source via `node-gyp` during install. You
will need a C89 compiler and a C++17 compiler on your platform.

---

## Development

```sh
git clone https://github.com/Zyven-Labs/giljs.git
cd giljs
npm install
npm test
```

`npm test` runs the **libgil C test suite** (69 tests) followed by the **giljs
Node.js test suite** (14 tests) — both must pass.

---

## License

MIT — see [LICENSE](./LICENSE) and [libgil/LICENSE](./libgil/LICENSE).
