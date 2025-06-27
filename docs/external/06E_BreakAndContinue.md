# 06E: Break and Continue

Break and continue statements are now supported in Lox `while` and `for` loops:

```c++
for (var i = 0; i <= 5; i += 1){
    if (i == 2) continue;
    if (i == 4) break;
    print i;                // Expect: 0, 1, 3
}
```

Using the `break` or `continue` keywords outside of a loop is a syntax error.

The semantics of break and continue are as below:

|          | while                                               | for                                                                   |
| -------: | --------------------------------------------------- | --------------------------------------------------------------------- |
| break    | Jumps out of the while loop.                        | Jumps out of the for loop. The increment clause is not evaluated.     |
| continue | Loops to the evaluation of the `while` conditional. | Loops to the increment clause (empty statement if not defined).       |

Breaking out of or continuing for multiple loops is not supported.

For implementation details, refer to Documents [03I](../internal/03I_BreakAndContinue.md) and [04I](../internal/04I_LoopVariableClosure.md).