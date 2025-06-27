# 10E: Try-Throw-Catch Based Exceptions

The experimental branch `containers` supports `try`-`catch` clauses and `throw` statements.

```
fun fac(n){
    if (n < 0) throw "Error: n cannot be negative.";
    
    if (n == 0) return 1;
    else return n * fac(n-1);
}

fun outer(){
    var num;
    try {
        num = fac(5);
        num = fac(-1);
        return "Final value: ${num}";
    } catch(e) {
        print e;
        return "Last calculated value: ${num}";
    }
}

print outer();
// Error: n cannot be negative.
// Last calculated value: 60
```

Note that a `try` clause must always be paired with a `catch` clause. Failure to do so is a compile error.

Any value can be thrown, though it is usually a string descriptor. This is also colloqually referred to as throwing a runtime exception. Native functions, as well as user-defined functions, are able to throw runtime exceptions.

Throwing unwinds the call stack up to the point where a try block is encountered, at which point the catch clause is excecuted. Not catching a thrown object promotes the runtime exception to a fatal runtime error.

Refer to Document [14I](../internal/14I_Exceptions.md) for implementation details.