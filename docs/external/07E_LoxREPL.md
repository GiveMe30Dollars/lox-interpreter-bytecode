# 07E: Lox REPL

The Lox REPL now supports evaluating expressions. The result of the evaluation, if not `nil`, is automatically printed onto the REPL output.

```
>>> var a = 3;
>>> var b = 7;
>>> a + b
10
```

This functionality is only available in REPL mode. Expressions not terminated with `;` are considered to be syntax errors in Lox files.

Statements must still be terminated with `;`.