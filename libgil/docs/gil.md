# Gil (Gnosis Intent Language) v1 Specification

## 1. Overview

Gil is a language for defining intents that transform a Gnosis frontier.

A Gil program defines intents. An intent is a named transition that evaluates against the current frontier and produces a new frontier.

Gil uses synchronous, non-blocking assignment semantics. Assignments generated during an intent execution are collected and committed atomically.

---

## 2. Frontier Model

The frontier contains predicate values.

A predicate represents a proposition identified by a name and zero or more arguments:

```gil
alive
location[alice, room1]
owns[alice, sword]
```

Predicate values are:

- `true`
- `false`
- `both`

A predicate not present in the frontier has the value `false`. The value `both` represents contradiction: the predicate is both true and false.

---

## 3. Syntax (BNF)

```bnf
program            ::= { intent_declaration }

intent_declaration ::= "intent" identifier "(" parameter_list? ")"
                       { statement }
                       "end"

parameter_list     ::= identifier { "," identifier }

statement          ::= assignment
                     | when_block

when_block         ::= "when" expression "do"
                       { statement }
                       "end"

assignment         ::= predicate "<=" expression

predicate          ::= identifier
                     | identifier "[" argument_list? "]"

argument_list      ::= identifier { "," identifier }

expression         ::= disjunction

disjunction        ::= conjunction { "or" conjunction }

conjunction        ::= negation { "and" negation }

negation           ::= "not" negation
                     | primary

primary            ::= value
                     | predicate
                     | "(" expression ")"

value              ::= "true"
                     | "false"
                     | "both"

identifier         ::= letter { letter | digit | "_" }
```

Operator precedence: `not` > `and` > `or`. Parentheses override precedence.

---

## 4. Predicates

Predicates with zero arguments may omit brackets:

```gil
alive                # equivalent to  alive[]
```

Predicate names are case-sensitive: `Location` and `location` are distinct.

---

## 5. Identifier Convention

- Identifiers beginning with uppercase letters are **variables**.
- Identifiers beginning with lowercase letters are **literal constants**.

Variables are bound by:

1. Intent parameters (declared in the intent header), or
2. Pattern matching in `when` clauses.

Example:

```gil
intent move(Player, Destination)
    when location[Player, Current] do
        ...
    end
end
```

Here `Player` is bound by the intent header and `Current` by the `when` clause.

**Unbound variable error**: if an expression references a variable with no enclosing binding, an error is emitted at load time.

---

## 6. Intent Execution

An intent executes iteratively until convergence:

1. Bind intent parameters from the invocation arguments.
2. Repeat until no assignments are produced:
   a. Evaluate all statements against the current frontier — the frontier is **not** modified during evaluation.
   b. Collect all generated assignments.
   c. Resolve assignments per Section 10 (Assignment Resolution).
   d. Commit assignments atomically to produce the next frontier.
3. If the frontier changed, repeat from step 2.
4. Terminate when no assignments are produced (stable state).

Multiple intent invocations execute sequentially in the order they are received. Each invocation must reach a stable state before the next begins.

Termination is the intent author's responsibility. The language provides no iteration limit, cycle detection, or timeout.

---

## 7. Statements

A statement is either an assignment or a `when` block. Statements may appear anywhere inside an intent or `when` block. Nested `when` blocks are permitted.

---

## 8. When Blocks

Syntax:

```gil
when expression do
    statements
end
```

A `when` block executes when its expression evaluates to `true` or `both`. It does not execute when its expression evaluates to `false`. If no frontier predicates match the `when` condition, the block contributes no assignments (a silent no-op).

---

## 9. Assignment Semantics

Assignments use the non-blocking operator:

```gil
predicate <= expression
```

Assignments do not modify the frontier immediately; they define values for the next frontier.

Multiple assignments to the same predicate are accumulated and resolved at commit time. Assignments accumulate across iterations until commitment; each iteration resets assignment collection from scratch.

---

## 10. Assignment Resolution

When multiple assignments target the same predicate, their values combine as follows:

```gil
true  + false = both
true  + both  = both
false + both  = both
both  + both  = both
```

Each combination produces exactly one result:

- two identical values (`true + true` or `false + false`) yield that value
- any contribution of `both`, or a `true`/`false` conflict, yields `both`

---
## 11. Expression Semantics

Expressions evaluate to `true`, `false`, or `both`.

Operators:

- `not` — unary negation
- `and` — logical conjunction
- `or` — logical disjunction

---

## 12. Truth Tables

### NOT

| A     | not A |
|-------|-------|
| true  | false |
| false | true  |
| both  | both  |

### AND

| A     | B     | A and B |
|-------|-------|---------|
| true  | true  | true    |
| true  | false | false   |
| true  | both  | both    |
| false | true  | false   |
| false | false | false   |
| false | both  | false   |
| both  | true  | both    |
| both  | false | false   |
| both  | both  | both    |

### OR

| A     | B     | A or B |
|-------|-------|--------|
| true  | true  | true   |
| true  | false | true   |
| true  | both  | true   |
| false | true  | true   |
| false | false | false  |
| false | both  | both   |
| both  | true  | true   |
| both  | false | both   |
| both  | both  | both   |

---

## 13. Example

The following intent activates every node reachable from a starting node through a `connected` relation:

```gil
intent propagate_active(Node)

    activated[Node] <= true

    when activated[A] do
        when connected[A, B] do
            activated[B] <= true
        end
    end

end
```

Given the frontier:

| Predicate                | Value |
|--------------------------|-------|
| activated[start]         | true  |
| activated[middle]        | false |
| activated[end]           | false |
| connected[start, middle] | true  |
| connected[middle, end]   | true  |

Executing `propagate_active(start)` repeatedly applies the `when` clauses until no predicate value changes. Because the language commits assignments atomically and iterates to a fixed point, `activated` becomes `true` for every node reachable from `start`, so `activated[middle]` and `activated[end]` each converge from `false` to `true`.

---

## 14. Summary

Gil v1 defines:

- Predicates as frontier propositions
- Intents as transitions
- When blocks as conditional execution
- `<=` as synchronous non-blocking assignment
- Atomic commit as a frontier transition
- Iterative execution until convergence

Fundamental operation:

```gil
Intent + Frontier -> Next Frontier
```

---

## 15. Termination Guarantee

Intent programs must be written to guarantee termination under all possible frontiers.

The runtime does not enforce:

- a maximum iteration count
- cycle detection
- timeout limits

Authors must ensure their intent implementations converge by:
- designing state transitions toward a fixed point
- avoiding cyclic dependencies between predicates
- ensuring conditional guards eventually become false
