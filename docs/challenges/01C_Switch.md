# 01C: Switch

### Note: this has not an implementation guide, just me brainstorming.

I'm using this as a sounding board for myself, so that anything I dreamt up wouldn't be lost to the aether.  
*Aside: I might be able to reuse this page for the write-up when I do get it up and running, so there's also that. It's why I'm not just stuffing this into 18-22C.md.*

I planned to use a Lox HashTable as a jump table. I'd create it during compile-time and stuff it into the constants array. Then, simply use a new opcode `OP_JUMP_TABLE` `const_idx` to search for the right value during runtime. Use a new singleton value `VAL_DEFAULT_CASE` for the default block.

There are... issues.

One limmitation is that case operands *must* be constants (ie. literals). I cannot programatically create a variable `a` and use that as a case, it would throw a compile error.

However, consider:

```
var b = "string"
switch (str){
    case "interpolated ${b}": //...
    // ...
}
```

A string interpolation desugars to `concatenate` and `string` native function calls. This *looks* like a literal but isn't. Should this be allowed? Limiting this seems... not great. 

Pattern-matching functions like `function(str)` or even `Instance is ClassType` that returns a true/false value might also be useful, though that is... hard and would be no better than an if-else chain. Maybe add a special native function that returns whatever is passed if it clears the type and `VAL_EMPTY` otherwise?

If I want this, I need to support expressions for the jump table. Since I cannot evaluate expressions during compile-time, it *has* to be done during runtime.

I also want to allow fallthrough for empty cases. Though, fallthrough with partially-filled cases seems... [evil](https://isocpp.org/wiki/faq/big-picture#defn-evil). Orphaned local variables on the stack will come back to bite us in the ass later, so each case body needs to clean the stack when it's done executing.

## What then?

An apporach I could imagine is this:
- When a switch is compiled, save the identifier used and emit `OP_CONSTANT` to load an empty jump table onto the stack.
- Create and initialize a LoopInfo struct. We'll be using their endpatches for end of case statement breaking.
  *Aside: even if I don't let the Lox end-user use `break` or `continue`, this is basically how it'll work in the bytecode. It's basically a one-iteration loop.*
- Filling of jump table:
  - Create *another* LoopInfo. This is for the ip to the case body.
  - While match(`TOKEN_CASE`), evaluate the expression. Then, emit `OP_DEFINE_CASE`, which pops it off the stack and into the jump table to be endpatched.
  - Emit `OP_JUMP` past the case body.
  - End the LoopInfo, endpatching all `OP_DEFINE_CASE` to case body.
  - Evaluate *one statement*. To prevent local variable shenanigans, inline declarations are not allowed. Block scope is required for multiple statements.
  - Emit pre-jump pops and emit `OP_JUMP` to be endpatched.
  - Continue until `TOKEN_RIGHT_BRACE` or (optional) `TOKEN_DEFAULT` encountered:
  - Same thing, but emit `OP_DEFAULT_CASE` before `OP_DEFINE_CASE`.
- Backpatch the `OP_JUMP` we emitted earlier.
- Emit `OP_POP` to pop the jump table off the stack.
- Emit `OP_GET_VARIABLE` for the saved identifier.
- Emit `OP_JUMP_TABLE`, which attempts to get the jump table entry of the top of stack. If successful, jump to that position.
- End the LoopInfo struct, backpatch all endpatches for case body breaks to here.
- Emit `OP_POP` to pop off the identifier.


This is fine, except its even less performant than if-else chains because all conditionals are evaluated. Hmmmmmmmmmmmmmmm.