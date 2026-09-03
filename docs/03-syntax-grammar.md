## 4. Language Syntax and Grammar

### 4.1 Basic Syntax Elements

Gil is defined by a formal grammar that specifies acceptable program structures. The language follows a combination of imperative programming constructs with logical elements.

### 4.2 Grammar Structure

The grammar is defined in Extended Backus-Naur Form (EBNF):

```
program            ::= intent_definition*

intent_definition  ::= "intent" intent_name parameter_list "do" statement_list "end"

parameter_list     ::= "(" parameter_list_contents ")" | epsilon
parameter_list_contents ::= parameter ("," parameter)* | epsilon
parameter          ::= IDENTIFIER

statement_list     ::= statement*

statement          ::= assignment
                    | when_block
                    | repeat_block
                    | expression

assignment         ::= predicate "<=" expression

predicate          ::= IDENTIFIER ("[" argument_list "]")?

argument_list      ::= argument ("," argument)* | epsilon
argument           ::= IDENTIFIER | INTEGER_CONSTANT

when_block         ::= "when" expression "do" statement_list "end"
repeat_block       ::= "repeat" statement_list "end"

expression         ::= or_expression
or_expression      ::= and_expression ("or" and_expression)*
and_expression     ::= not_expression ("and" not_expression)*
not_expression     ::= "not" not_expression
                    | primary_expression
primary_expression ::= "(" expression ")"
                    | predicate
                    | INTEGER_CONSTANT
                    | IDENTIFIER
```

### 4.3 Lexical Elements

**Identifiers**: 
- Begin with a letter (a-z, A-Z) or underscore (_)
- Followed by letters, digits, or underscores
- Examples: `foo`, `bar_123`, `myPredicate`
- Identifiers starting with uppercase are variables; lowercase are literals

**Integer Constants**:
- Decimal numbers
- Used only as predicate arguments
- May be used in arithmetic expressions

### 4.4 Assignment Operations

Gil uses the `<=` operator for assignments:
- `predicate <= value`

Assignment is non-blocking, collected during execution, then resolved and committed atomically.

### 4.5 Control Structures

Gil includes several control structures:
- **When blocks**: Conditional statements that execute only when their condition is true
- **Repeat blocks**: Iterative constructs that continue until convergence
- **Expression logic**: Boolean operations with proper operator precedence

These structures enable complex logic flow while maintaining the deterministic nature of Gil.