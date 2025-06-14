# 04I: Loop Variable Closure for For Loops

This is part of a Challenge description for Chapter 25. The original solution can be found here: https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter25_closures/2.md.

There's nothing too significant to add here. Most of the groundwork has been laid out by Nystrom in the original solution: create an inaccessible loop variable outside the scope of the loop block, then an *inner* variable that shadows it and uses its name. Every time an iteration concludes, reassign the loop variable to the value of the inner variable in case the loop block modified it, before popping the inner variable and jumping to the increment clause. This way, closures capture the inner variable and not the mutating loop variable.

Simple, were it not the fact I added keywords to bypass the usual control flow of loops. Whoops.

## Remembering The Loop and Inner Variables Exist

Recall from [Document 03I](03I_BreakAndContinue.md) that:
- `break` adds an `OP_JUMP` to the end of the loop body, which is backpatched when the loop ends. The instructions to be backpatched stored in `loopInfo->endpatches`.
- `continue` adds an `OP_LOOP` to the start of the loop or the increment clause of a for loop.
- Both `break` and `continue` emit `OP_POP` to clear the stack of local variables in the loop body before jumping.

This doesn't account for the closure system; both the existence of the inner variable, as well as local variables being closure objects that are *then* assigned to global variables so that they remain accessible even after the loop terminates.

First things first: the loop and inner variables.

Either we:  
a. Rewrite the jump logic for break and continue so that they always go to the end of the loop, or  
b. Let break and continue emit the bytecode to handle the loop and inner variables before jumping.

Seeing that I already done `b.` for emitting the bytecode for clearing the local variable stack, let's continue with that approach.

Break and continue now need access to the stack positions of the loop and inner variables, so we stuff that into the LoopInfo struct.

```
typedef struct LoopInfo {
    int loopStart;
    int loopDepth;
    int loopVariable;              // <-- Added 2 lines
    int innerVariable;             // <--
    struct LoopInfo* enclosing;
    ValueArray endpatches;
} LoopInfo;
```

Since these fields are specific to `for` loops, I initialize them to `-1` (indicating none), and just manually assign them during `for` loop parsing.

*Aside: You could argue that only one of the two stack positions need to be recorded, as the inner variable is always the next local variable to the loop variable. **You would be correct.** However, struct padding and alignment means that the space taken up by the struct would be the same anyways.*

## Loop and Inner Variable Handling

We already know how to assign the loop variable to the value of the inner variable:

```
// in continueStatement():
    // assign inner variable to loop variable (if any)
    if (current->loop->loopVariable != -1){
        emitBytes(OP_GET_LOCAL, (uint8_t)current->loop->innerVariable);
        emitBytes(OP_SET_LOCAL, (uint8_t)current->loop->loopVariable);
        emitByte(OP_POP);
    }
```

However, after popping the local variables, we also need to get rid of the inner variable before we jump. Otherwise, the loop never terminates; the loop variable is assigned to an orphaned inner value after every iteration.

I first naively thought to just pop the variable. But isn't the whole point of this change to allow the inner variable to be captured by a closure? So, whether to emit `OP_POP` or `OP_CLOSE_UPVALUE` depends on whether the inner variable is captured:

```
// in continueStatement()
    // remove inner variable (if any): OP_CLOSE_UPVALUE if captured, OP_POP otherwise
    if (current->loop->loopVariable != -1)
        emitByte(current->locals[current->localCount - 1].isCaptured ? OP_CLOSE_UPVALUE : OP_POP);
```

This is repeated in `breakStatement`, though since we're breaking out of the entire loop, it is unecessary to assign to the loop variable. It's getting removed anyways.

## Oops, I Forgotten About Upvalues on the Loop Stack

Consider the following:

```
var global;
for (;;){
    var a = 3;
    var closure = fun(a) {a * n};
    global = closure;
    break;
    // Hey did we forget something?
}

{
    var b = 4;      // Hey I think we just overwritten something important
    global(5);      // Uhhhhh
}
```

When we hit `break`, we assign to the loop variable (there isn't any), emit pre-jump pops (two, for `a` and `closure`), then break out of the loop.

We never closed the upvalue `a` when we break. So far, the only function that emits `OP_CLOSE_UPVALUE` is `endScope`. This wouldn't be a problem if `closure` is inaccessible, but it's assigned to `global` before we exited scope.

Calling `global` now would be invoke undefined behaviour: specifically, this call would return 20 as the stack slot for open upvalue `a` has the value `b = 4`.  
More likely, the local that eventually ends up in that slot is an incompatible data type, and calling `global` leads to a runtime error.

Yeah....

This was admittedly a pretty simple fix. In `emitPrejumpPops`, check whether the variable is captured, and emit `OP_CLOSE_UPVALUE`. This is essentially a copy-paste of `endScope` without modifying the locals array:

```
static void emitPrejumpPops(){
    // iterates through locals (does not affect it!), emits pops for values within the loop
    // if the loop has a captured value, emit OP_CLOSE_UPVALUE instead
    // (scope depth higher but not including depth at which loop is created)
    int pops = 0;
    for (int i = current->localCount - 1; i >= 0; i--){
        if (current->locals[i].depth > current->loop->loopDepth){
            if (current->locals[i].isCaptured){
                emitPops(pops);
                pops = 0;
                emitByte(OP_CLOSE_UPVALUE);
            } else {
                pops++;
            }
        } else break;
    }
    emitPops(pops);
}
```

In fact, you can think of `break` and `continue` as a conditional ending of the scope of the loop body during runtime. However, we do still need to keep the `locals` contents around for the remainder of the loop that *doesn't* exit. I admit that the name `emitPrejumpPops` is less accurate now for what this function does. Oh well.

I only noticed this when I was wiring up the logic for closing the inner variable if captured. Quick oversight that nonetheless demonstrates the importance of testing (of which I'm not doing much of, admittedly).