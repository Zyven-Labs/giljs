## Appendix A: Grammar Reference

### A.1 Complete BNF Grammar

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

condition          ::= predicate ("[" argument_list "]")?
```

### A.2 Lexical Rules

**Identifiers**:
- Start with letter or underscore
- Followed by letters, digits, or underscores
- Length limited by implementation
- Uppercase-starting identifiers are variables; lowercase-starting are literals


**Integer Constants**:
- Decimal representation only
- Used in argument positions only
- Can be used in arithmetic expressions

### A.3 Operator Precedence

From highest to lowest precedence:
1. Parentheses `()`
2. Unary NOT `not`
3. Multiplication `*` and Division `/`
4. Addition `+` and Subtraction `-`
5. Logical AND `and`
6. Logical OR `or`

### A.4 Reserved Words

The following words are reserved and cannot be used as identifiers:
- `intent`
- `do`
- `end`
- `when`
- `repeat`
- `not`
- `and`
- `or`
- `both`
- `true`
- `false`