## 6. Three-valued Logic

### 6.1 Truth Table Overview

Gil uses standard three-valued logic with the following operations:

**NOT Operation**:
```
NOT false = true
NOT true  = false
NOT both  = both
```

**AND Operation**:
```
true  AND true  = true
true  AND false = false
true  AND both  = both
false AND true  = false
false AND false = false
false AND both  = false
both  AND true  = both
both  AND false = false
both  AND both  = both
```

**OR Operation**:
```
true  OR true  = true
true  OR false = true
true  OR both  = true
false OR true  = true
false OR false = false
false OR both  = both
both  OR true  = true
both  OR false = both
both  OR both  = both
```

### 6.2 Special Values

The `both` value represents a logical contradiction:
- When a predicate has value `both`, the system has detected inconsistent information
- It propagates outward through logical operations in a predictable manner
- Used for modeling real-world situations with contradictory evidence

### 6.3 Variable Semantics

Variables in Gil differ from typical programming variables:
- They start as unbound (no value)
- Bound through parameter passing or pattern matching
- Once bound, they can appear anywhere in the notation as their concrete value
- Achieve all the power of variables without the complexity of assignment statements

Variables can be:
- **Upper case identifiers**: Represent variables (e.g., `Person`, `Room`)
- **Lower case identifiers**: Represent literals (e.g., `my_room`, `foo`)
- **Match patterns**: Used for pattern matching queries

### 6.4 Applications of Three-valued Logic

Three-valued logic enables Gil to model:
- **Contradictory information**: When multiple sources conflict
- **Uncertain states**: When definitive information is unavailable
- **Incomplete systems**: When not all facts are known
- **Multi-source reasoning**: Combining information from disparate sources