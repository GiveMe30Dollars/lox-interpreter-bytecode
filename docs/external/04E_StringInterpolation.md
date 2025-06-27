# 04E: String Interpolation

String interpolation is the insertion of expressions into strings:

```
var b = true;
print "Interpolation is ${b}!";  // Interpolation is true!
```

String interpolations begin with the symbols `${` in a string, and are terminated by `}`, after which the rest of the string is read normally.

String interpolations also support nesting and chaining:

```kotlin
print "Nested ${ "interpolations? ${"Why??"}" }";
print "Chaining ${"seems"} more ${"commonplace"} though.";
// Nested interpolations? Why??
// Chaining seems more commonplace though.
```

*Note:* String interpolation is syntactic sugar for function calls to native functions `string` and `concatenate`. These two lines are equivalent:

```kotlin
var drink = "Tea";
var steep = 4;
var cool = 2;

print "${drink} will be ready in ${steep + cool} minutes.";
print concatenate(string(drink), " will be ready in ", string(steep + cool), " minutes.");
```

Implementation details limit the maximum number of nested braces to be 256. This includes braces used in block quotes *and* string interpolations. For further details, refer to Document [02I](../internal/02I_BraceScanning.md).