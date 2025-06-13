# 05E: Anonymous Functions

Anonymous functions can now be written:

```
var product = fun (m, n) { 
    return m * n; 
};
print product(3, 2)    // 6.
```

Note that due to existing Lox syntax rules, an anonymous function as a standalone expression is invalid syntax.  
(And you couldn't do much with it anyways because you cannot call it, because it isn't bound to an identifier or assigned to a variable.)

```
fun (m, n){
    return m + n;
};
// Error at '(':  Expect function name.
```

Anonymous functions also support closures.

Refer to document 05I for implementation details.