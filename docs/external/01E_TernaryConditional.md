# 01E: Ternary Conditional

### Why Not Ternary Operator?

The ternary operator generaly describes [any operation that takes in three operands](https://en.wikipedia.org/wiki/Ternary_operation#Computer_science). However, due to the proliferation of C-family languages, people usually refer to the ternary conditional when they talk about "the ternary operator", "conditional" or variants.

As this implementation of Lox is soon to add other ternary operators and even quaternary operators (most clearly in array slicing, `a[b:c]` and `a[b:c:d]`), the term ternary conditional is to be used to clearly disambugate them.

### Semantics

> The detailed semantics of "the" ternary operator as well as its syntax differs significantly from language to language.  
*\- Egregious understatement, [Wikipedia](https://en.wikipedia.org/wiki/Ternary_conditional_operator).*
<br>
Most languages agree on the following:

- Ternary conditionals short-circuit.
  - `false ? func1() : func2()` will never call `func1()`. Only one of the branches is executed at any time.
- *Generally*, ternary conditionals permit side effects (function calls etc.).
  - This is generally true for most languages in use today, though early procedural languages tend to disallow side effects.
- *Generally*, ternary conditionals are right-associative.
  - PHP notably (and notoriously) doesn't. `a == 1 ? "one" : a == 2 ? "two" : "many"` is evaluated as `(a == 1 ? "one" : a == 2) ? "two" : "many"`, which most agree is unintuitive.
- *Generally*, ternary conditionals have low binding power, second lowest to assignment (and compound assignment).
  - C++, unlike its predecessor C, has ternary conditionals have the same binding power as assignment.
- *Generally*, ternary conditionals only return rvalues.
  - C++, unlike C or C#, allows ternary conditionals to return lvalues.
    - Hence, `(cond ? a : b) = value` compiles in C++ and not C or C#.  
    The only other language (to my knowledge) that could return lvalues from ternary conditionals is JavaScript.  
    *Aside: idk man shits weird*
- *Generally*, nested ternary conditionals are evaluated starting from the outermost expression, proceeding inwards.
  - If the opposite were true (nested conditionals are evaluated starting from the innermost expression spreading outwards), then  
  `bool b = false;  int c = b ? 10 : (true ? (b = true) : 0);` evaluates to `10` instead of `true -> 1`.

Notable, Go does not implement the ternary conditional. Python uses a different syntax from `?:`.  
Rust removed its ternary conditional due to duplication with `if` `else`, which can be used as an expression.

Further reading: https://en.wikipedia.org/wiki/Ternary_conditional_operator

## Ternary Conditional in Lox

In this implementation of Lox, ternary conditionals:

- Short-circuit.
- Permit side effects.
- Are right-associative.
- Have low binding power, second lowest to assignment.
- Evaluate starting from the outermost expression, proceeding inwards.
- Are nestable without parentheses.
- Have branches that have at most `PREC_CONDITIONAL` precedence.
  - `a = cond ? b = b + 1 : c ;` will not compile.  
  Use parentheses instead: `a = cond ? (b = b + 1) : c;`
- Only returns values, and not assignable variables.
  - `(cond ? a : b) = value` will not compile.

## Optionally Ternary

A binary equivalent to the ternary conditional (also known as the [Elvis operator](https://en.wikipedia.org/wiki/Elvis_operator)) is also implemented.

Note that there exists a distinction between the ternary conditional and Elvis operator, particularly for side effects:

- `a ? : b` is equivalent to `a ? a : b`.
- `func1() ? : func2()` is similar to `func1() ? func1() : func2()`, but func1 is only called once if it evaluates true.

Note that behaviour may differ depending on the `IS_FALSEY_EXTENDED` flag.