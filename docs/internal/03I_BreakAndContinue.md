# 03I: Break and Continue

This.. was a doozy.

This write-up assumes you have read [the second part of Crafting Interpreters](https://craftinginterpreters.com/a-bytecode-virtual-machine.html) up until [Chapter 24: Calls and Functions](https://craftinginterpreters.com/calls-and-functions.html#call-frames). This is the state of the project when I decided to add `break` and `continue` statements.

Nystrom has kindly provided reference answers [here for `continue` implementation](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter23_jumping/2.md) and [here for an AST-based `break` implementation using Java Exceptions.](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter09_control.md)

## Prelude

I knew that I already had all the tools to implement break and continue. The main issue came with preserving state information.

`continue` is easy. We only need the offset to the loop start to emit `OP_LOOP`.

`break` statements are effectively goto's to the end of a loop. That means when compiling them, we don't know *where* the `OP_JUMP` operands should point to. So, we need to store all of the break instructions *somewhere*, and backpatch them when we eventually compile the end of the loop.

Also, we cannot discard this information whenever a new loop is made. *Loops nest*. We have to retain information even as the inner loop shadows the outer one. You could say that... loop information has stack semantics...

## Yeah This Is `Compiler*` All Over Again

```
typedef struct LoopInfo {
    int loopStart;
    int loopDepth;
    struct LoopInfo* enclosing;
    ValueArray endpatches;
} LoopInfo;
```

Behold, the `LoopInfo` struct.

The name LoopInfo might be a bit misleading. While the main logic behind compiling while and for statements are encoded within their respective functions, this represents the state of being *in* a loop. The information required by break and continue is stored here.

In order:
- `loopStart`: chunk code offset to the beginning of the loop. This is where `continue`'s `OP_LOOP` instructions point to.
- `loopDepth`: the scope depth where the loop is made. This information becomes important later.
- `enclosing`: the enclosing loop. if this is the first loop, enclosing is `NULL`.
- `endpatches`: the dynamic array of chunk code offsets referring to an `OP_JUMP` instruction that is to be backpatched to the end of the loop.  
  *Aside: You'd be right to question whether stuffing a `size_t` (or an `int` for that matter) into a `double` would cause any conversion inaccuracies, since there is no Value representation for integers. I had such doubts myself. However, IEEE-754 does specify the number of mantissa (read: non-exponent) bits of a double to be 52, well over the 32 bits an `int` would have.*  
  *Get back to me when there are [4,294,967,296](https://cplusplus.com/reference/cstdint/) bytes in that bytecode chunk. I'll wait.*

Like `Compiler`, LoopInfo "instances" are never dynamically allocated. They are created in the functions `whileStatement` and `forStatement` as local structs, and we are done with them by the time those functions return. Everything else takes a pointer to them.

The Compiler has a new field added for the pointer to the current loop, `NULL`-initialized when a Compiler is first created:

```
typedef struct Compiler {
    // ...
    LoopInfo* loop;
    // ...
}
```

When entering a loop, a new LoopInfo is made and initialized:

- **`void initLoopInfo(LoopInfo* loopInfo, int loopStart)`**:
  - Initializes all fields to default values:
    - `loopInfo->loopStart`: parameter value.
    - `loopinfo->scopeDepth`: current scope depth.
    - `loopInfo->enclosing`: the LoopInfo pointer currently in the Compiler `current->loop`.
    - `loopInfo->endpatches`: dynamically allocated as an empty ValueArray.
  - Sets `current->loop` to this `loopInfo`.

Similarly, when we're exiting a loop, we call a "destructor":

- **`void endLoopInfo()`**:
  - Iterates through and backpatches all the jump instructions represented in `current->loop->endpatches` to current position (end of loop)
  - Frees the ValueArray `current->loop->endpatches`; this was dynamically allocated when we initialized it!
  - Restores the enclosing LoopInfo `current->loop->enclosing` as `current->loop`

## Pre-Jump Pops

Consider the state of the VM stack when the `break` statement is reached:

```
{
    var outer;
    while (true){
        var a;
        var b;
        {
            var c;
            break;
        }
    }
}
```

It'd look something a bit like this:

```
+----------+-------+-------+-------+-------+------ - -
| <script> | outer |   a   |   b   |   c   |  . . .  
+----------+-------+-------+-------+-------+------ - -
                                              |
                                              |
                                               stackTop
```

Before we jump out of the while loop, we need to "clean house" and pop off local variables a, b and c. Otherwise, they'll just *stay* on the stack, wreaking havoc whenever we try doing *anything* with local variables afterwards. This is true for both `break` and `continue`.

Because Lox does not allow declarations inline with control flow statements, the only place a local variable could be declared in is in a block. If that sounds like resolving local scope, *that's because it is*, and we already have a handy representation for that during compile-time:

```
typedef struct Compiler {
    // ...
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    // ...
}
```

We don't want to *alter* the `locals` array when we do this, since later compilation still needs the local variables — for instance, using an `if(...) break;` to exit a base case — so we'll need to define a new function rather than the existing ones for beginScope and endScope.

The function emitPrejumpPops does just that:

- **`void emitPrejumpPops()`**:
  - Iterate through the `current->locals` array in descending index order. If the scope depth is greater than the depth of the loop `current->loop->loopDepth`, increment local variable `pops`. Else, break.
  - Emit the corresponding opcodes for popping the loop local variables off the stack:
    - 0: Emit nothing.
    - 1: Emit `OP_POP`.
    - 2 or more: Emit `OP_POPN`, followed by a byte operand `2 - 255`.  
      *Aside:* this is fine since we can't have more than 255 local variables without overflowing the stack anyways.

## Putting It All Together

- In `whileStatement()`, 
  - Declare and initialize a new LoopInfo struct, before compiling the expression conditional.  
    You can do this any time before the body statement is compiled as long as you record `loopStart` to be here. I did so to make it clear that *the conditional is the loop start*. Any continue statements jump to the evaluation of the conditional.
  - After all the bytecode relating to `while` has been emitted, call `endLoopInfo()`.  
    The conditional is popped either before execution of the body statement or just after the loop is exited. If we're breaking from the while loop, there is no extra conditional on the stack. We should skip past the tail `OP_POP`.

- In `forStatement()`,
  - Declare and initialize a new LoopInfo struct *after* compiling the increment statement.  
    This is necessary. When we `continue` from a for loop, we want to go to the increment statement (if any) first before returning to conditional evaluation.
  - We call `endLoopInfo()` before the opcode for popping the iteration variable is emitted. Any breaks should go here so that the for loop can clean up after itself.

- Create new statement function `breakStatement()`:
  - If there is no existing loop, throw an error.
  - Consume the trailing semicolon.
  - Call `emitPrejumpPops()`.
  - Call `emitJump()`, and store the chunk code offset by calling `addEndpatches()`.

- Create new statement function `continueStatement()`:
  - If there is no existing loop, throw an error.
  - Consume the trailing semicolon.
  - Call `emitPrejumpPops()`.
  - Call `emitLoop()` using the current loop's starting position.

*Aaaaaaand That's It Folks!*

## Closing Thoughts