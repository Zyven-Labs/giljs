## 3. Core Concepts

### 3.1 Predicates

Predicates are propositions identified by a name and zero or more positional arguments:
- **Syntax**: `name[argument1, argument2, ...]`
- **Examples**: 
  - `alive` (no arguments)
  - `location[alice, room1]` (two arguments)
  - `owns[alice, sword]` (two arguments)

Predicates serve as the fundamental building blocks of Gil programs and represent statements about the system state. They can be:
- **Unary predicates**: Having no arguments (e.g., `alive`)
- **Multi-argument predicates**: Having one or more arguments (e.g., `location[alice, room1]`)

### 3.2 Frontiers

A frontier is a mutable set of predicate/value bindings. Every predicate absent from the frontier is implied to have a value of `false`. The frontier serves as the system state that intents manipulate.

Key characteristics of frontiers:
- **Mutable**: Can be modified through intent execution
- **Predicate-Based**: Each entry is a predicate-to-value mapping
- **Three-Valued**: Values can be true, false, or both
- **Efficient Lookup**: Implemented as hash tables for fast access

### 3.3 Intents

An intent is a named, parameterized block of Gil statements that transforms a frontier through execution. Each intent can be:
- **Parameterized** with name arguments
- **Executable** against a frontier
- **Deterministic** with predictable outcomes

Intents serve as system actions or transformations within the Gil model. They are defined with:
- A name (used for execution)
- Parameter list (for input arguments)
- Body containing statements to execute

### 3.4 Truth Values

Gil uses three-valued logic:
- `false` - The default state for absent predicates
- `true` - Indicates predicate is verified
- `both` - Indicates contradiction (predicate is both true and false)

The three-valued nature allows Gil to model:
- **Negation**: When negations create contradictions
- **Uncertainty**: When information cannot be completely resolved
- **Inconsistency**: When contradictory evidence is present