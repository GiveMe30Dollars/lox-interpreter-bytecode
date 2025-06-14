# 05E: Anonymous Functions (Lambdas)

Anonymous functions expressions, otherwise known as lambdas, start with the `fun` keyword, followed by parentheses enclosing a set of parameters, followed by braces enclosing a single expression for the body, the value of which is automatically returned.

```
var product = fun (m, n) { m * n };
print product(3, 2);    // 6.
```

You may use this to create a quick function to pass as an argument to another function call:

```
array.reduce( fun(a,b){a + b} );
```

Note that due to existing Lox syntax rules, an anonymous function as a standalone expression is invalid syntax.  
(And you couldn't do much with it anyways because you cannot call it, because it isn't bound to an identifier or assigned to a variable.)

```
fun (m, n){ m + n };
// Error at '(':  Expect function name.
```

Anonymous functions also support closures.

Refer to Document [05I](../internal/05I_Lambdas.md) for implementation details.