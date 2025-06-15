# 05E: Anonymous Functions (Lambdas)

Anonymous functions expressions start with the `fun` keyword, followed by parentheses enclosing a set of parameters, followed by braces enclosing a sequence of declarations.

Lambdas are a subset of anonymous function expressions that contains a single expression for the body, which is automatically returned upon calling.

```
var sum = fun (m, n) {
    // You can write any declarations or statements here.
    return m + n;
};
var product = fun (m, n) { m * n };    // Limited to one expression.
print sum(5, 2);        // 7
print product(3, 2);    // 6
```

You may use this to create a quick function to pass as an argument to another function call:

```
array.reduce( fun(a,b){a + b} );
```

Note that due to existing Lox syntax rules for defining named functions, an anonymous function or lambda as a standalone expression is invalid syntax:  

```
fun (m, n){ m + n };
// Error at '(':  Expect function name.
```

Consider wrapping the anonymous function in parentheses:

```
(fun (m,n){ m + n });
// This won't throw a compile error. It's just not particularly useful since we can't access it.
```

Unlike named functions, anonymous functions may be called as soon as they are defined using the typical call syntax `()`:

```
(fun () { print "Self-calling!"; })();
```

Anonymous functions, like named functions, supports closures.

Refer to Document [05I](../internal/05I_Lambdas.md) for implementation details.