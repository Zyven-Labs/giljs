## 7. Advanced Features

### 7.1 Integer Constants and Arithmetic

Gil supports integer constants and arithmetic operations in predicate arguments:
- Integer constants are folded to their decimal string forms at load time
- Arithmetic expressions are supported within argument positions only
- Constants can combine using +, -, *, and / operators
- Operator precedence: * and / bind tighter than + and -

**Examples**:
```
intent set_at(Pred, Loc)
    Pred[Loc] <= true
end

intent add_one(Who, What, N)
    item_count[Who, What, N] <= false
    item_count[Who, What, N + 1] <= true
end
```

Integer arithmetic is evaluated at execution time when variables are bound, enabling flexible computational predicates.

### 7.2 Pattern Matching

Gil implements sophisticated pattern matching for queries:
- **UPPERCASE** names are variables that match any concrete value
- **lowercase** names are literals that must match exactly
- Variables that repeat (e.g., `linked[A, A]`) match only predicates where both positions hold the same value

### 7.3 When Blocks and Guards

When blocks provide conditional execution:
```
when activated[A] do
    when connected[A, B] do
        activated[B] <= true
    end
end
```

When blocks can be nested and provide a way to express conditional state propagation with complex logical dependencies.

### 7.4 Repeat Blocks

Repeat blocks enable iterative state evolution:
```
repeat
    when active[Node] do
        when connected[Node, Other] do
            active[Other] <= true 
        end
    end
end
```

Repeat blocks converge to a stable state, handling propagation and synchronization that may take multiple iterations.

### 7.5 Variable Binding

Gil variables are bound through:
- Intent parameter passing
- Pattern matching in `when` clauses  
- Query result processing
- Buffering of variable binding through complex expressions