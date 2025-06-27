# 07E: Lox REPL

The Lox REPL supports evaluating expressions. The result of the evaluation, if not `nil`, is automatically printed onto the REPL output. This is due to all function calls implicitly returning `nil` unless specified otherwise.

```c++
>>> var a = 3;
>>> var b = 7;
>>> a + b
10
```

This functionality is only available in REPL mode. Expressions not terminated with `;` are considered to be syntax errors in Lox files.

Statements must still be terminated with `;`.

There are two additional terminal commands that are exclusively available in the Lox REPL:

- `exit`: Exits the REPL session.
- `reset`: Resets the state of the VM. All global variables, user-defined functions and classes are deleted.