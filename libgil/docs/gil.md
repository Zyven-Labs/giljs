# Gil (Gnosis Intent Language) Specification

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

argument_list      ::= arg_value { "," arg_value }

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

arg_value          ::= identifier
                     | arithmetic

arithmetic         ::= term { ("+" | "-") term }

term               ::= factor { ("*" | "/") factor }

factor             ::= integer_literal
                     | variable
                     | "(" arithmetic ")"

variable           ::= identifier   # uppercase initial (see §5)

integer_literal    ::= non_zero_digit { digit }
                     | "0"

digit              ::= "0" | non_zero_digit

non_zero_digit     ::= "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"

identifier         ::= letter { letter | digit | "_" }
```

Operator precedence: within a predicate argument, `*` `/` bind tighter than `+` `-`. Logical operators (`not` > `and` > `or`) apply only to expressions and never mix with arithmetic. Parentheses override precedence in both.

---

## 4. Predicates

Predicates with zero arguments may omit brackets:

```gil
alive                # equivalent to  alive[]
```

Predicate names are case-sensitive: `Location` and `location` are distinct.

Predicate arguments may be identifiers (matching the identifier convention — see §5) or arithmetic expressions that evaluate to an integer constant:

```gil
score[alice, 42]
score[alice, 2 + 3]
score[alice, 2 + 3 * 4]   # 2 + 12 = 14
```

Integer arguments are constants matched literally; they follow the same equality semantics as string arguments. Arithmetic in argument positions is folded to a single integer constant at load time.

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

## 6. Integer Constants and Arithmetic

Integer constants are sequences of decimal digits:

```gil
0
42
1024
```

Integers are unbounded in the language; practical limits are enforced by the runtime. An integer constant evaluates to its numeric value.

Integer constants appear **only** as predicate arguments (see §4). They are never assigned with `<=` and are never converted to a truth value — integers and logical values (`true`, `false`, `both`) belong to disjoint domains and never mix.

Within a predicate argument, arithmetic operators combine integer constants and variables bound to integer constants:

- `+` — integer addition
- `-` — integer subtraction
- `*` — integer multiplication
- `/` — integer division (truncates toward zero)

```gil
position[alice, 3 + 4]        # matches position[alice, 7]
position[alice, 2 + 3 * 4]    # matches position[alice, 14]
position[alice, (2 + 3) * 4]  # matches position[alice, 20]
counter[N + 1]                # matches counter[<N+1>] when N is bound
```

Arithmetic with only integer constants is folded into a single constant at load time. Arithmetic that references a variable is evaluated at execution time against that variable's bound value.

Arithmetic operands must be integer constants or variables whose bound value is an integer constant. An identifier that is not an integer, a predicate, or a logical value in an arithmetic position is a type error.

Division by zero is a load-time error when the divisor is a constant; when the divisor is a variable that evaluates to zero, it is a runtime error (the intent fails and the frontier is left in its last committed state).

A variable used in arithmetic must be bound (by an intent parameter or a `when` clause) to an integer string at execution time. If it is bound to a non-integer value, execution fails.

---

## 7. Intent Execution

An intent executes iteratively until convergence:

1. Bind intent parameters from the invocation arguments.
2. Repeat until no assignments are produced:
   a. Evaluate all statements against the current frontier — the frontier is **not** modified during evaluation.
   b. Collect all generated assignments.
   c. Resolve assignments per Section 11 (Assignment Resolution).
   d. Commit assignments atomically to produce the next frontier.
3. If the frontier changed, repeat from step 2.
4. Terminate when no assignments are produced (stable state).

Multiple intent invocations execute sequentially in the order they are received. Each invocation must reach a stable state before the next begins.

Termination is the intent author's responsibility. The language provides no iteration limit, cycle detection, or timeout.

---

## 8. Statements

A statement is either an assignment or a `when` block. Statements may appear anywhere inside an intent or `when` block. Nested `when` blocks are permitted.

---

## 9. When Blocks

Syntax:

```gil
when expression do
    statements
end
```

A `when` block executes when its expression evaluates to `true` or `both`. It does not execute when its expression evaluates to `false`. If no frontier predicates match the `when` condition, the block contributes no assignments (a silent no-op).

---

## 10. Assignment Semantics

Assignments use the non-blocking operator:

```gil
predicate <= expression
```

Assignments do not modify the frontier immediately; they define values for the next frontier.

Multiple assignments to the same predicate are accumulated and resolved at commit time. Assignments accumulate across iterations until commitment; each iteration resets assignment collection from scratch.

---

## 11. Assignment Resolution

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
## 12. Expression Semantics

Expressions evaluate to `true`, `false`, or `both`.

Operators:

- `not` — unary logical negation
- `and` — logical conjunction
- `or` — logical disjunction

Precedence (highest to lowest): `not` > `and` > `or`. Parentheses override precedence.

Integer arithmetic is **not** part of expressions; it is available only inside predicate arguments (see §6). An integer constant or arithmetic expression cannot appear as a `when` condition or as the right-hand side of `<=`, and it is never converted to a logical value.

---

## 13. Truth Tables

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

## 14. Example

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

## 15. Summary

Gil defines:

- Predicates as frontier propositions
- Intents as transitions
- When blocks as conditional execution
- `<=` as synchronous non-blocking assignment
- Atomic commit as a frontier transition
- Iterative execution until convergence
- Integer constants and arithmetic, matched as predicate arguments

Fundamental operation:

```gil
Intent + Frontier -> Next Frontier
```

---

## 16. Termination Guarantee

Intent programs must be written to guarantee termination under all possible frontiers.

The runtime does not enforce:

- a maximum iteration count
- cycle detection
- timeout limits

Authors must ensure their intent implementations converge by:
- designing state transitions toward a fixed point
- avoiding cyclic dependencies between predicates
- ensuring conditional guards eventually become false
