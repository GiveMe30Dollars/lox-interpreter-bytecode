# 05I: Lambdas (Anonymous Functions)

*Aside: I know that they're not technically called that, but it's a nice and catchy name lol*  

Anonymous functions are really just functions under the hood. In fact, Lox functions do not need to inherently bind to a name, despite the `name` field in `ObjFunction` representation. The name-binding happens *after* `OP_CONSTANT` or `OP_CLOSURE` wrought up the function from the constants array and put it onto the stack. Then, we either bind it to a global identifier or leave it as-is for a local variable.

I could've left the name field blank.

But then there's nothing to print. There would be very little differentiating two anonymous functions other than the bytecode chunk itself. That doesn't make for a very pleasant user experience.

So I made up garbage to put as the name.

## <fn [4b95f515]>

Earlier, when I implemented FNV-1a for [Hash Tables](https://craftinginterpreters.com/hash-tables.html), I diverged from Nystrom's implementation by taking in a `uint8_t` pointer:

```
uint32_t hashBytes(const uint8_t* key, size_t numOfBytes) { /*...*/ }
```

This doesn't make a difference for strings, but I was also able to reuse this code for `double`, and now this.

`uint32_t` is 4 bytes. We can feed a hash into the hashing algorithm *and get a new hash.*

In this way, pseudo-random values can be generated from the hashing algorithm. We can then use `snprintf` to print it as an eight-digit hexadecimal, plus formatting. Since we know the length of the string, we can pre-allocate the array and only call `snprintf` once, knowing (reasonably certainly) that the operation will succeed. Then, we pass the raw character array and let `takeString` handle the rest.

To prevent two anonymous functions having the same hash, we wrap the logic in a do-while loop and check against the strings hash table. Realistically, we're not going to hit this condition; FNV-1a is a good-enough hashing function that most calls take no more than one iteration.  
*Aside: this does mean that using square-bracketed eight-digit hexadecimal strings in the program may (mildly) affect the efficiency of this function, but I don't think we're approaching any significant fraction of 16,777,218 combinations any time soon.*

Here it is, in all its glory:

```c
// in object.c, declared in object.h

ObjString* lambdaString(){
    const int length = 10;
    char* buffer = ALLOCATE(char, length + 1);
    do {
        vm.hash = hashBytes((uint8_t*)&vm.hash, 4);
        snprintf(buffer, length + 1, "[%08x]", vm.hash);
    } while (tableFindString(&vm.strings, buffer, length, vm.hash) != NULL);

    return takeString(buffer, length);
}
```

When initializing the VM, `vm.hash` is initialized to 0. Since nothing else uses this field, the first anonymous function generated will be `<fn [4b95f515]>`. 

Title card!

*Update:* in experimental branch `containers`, lambdaString has been rewritten to emit strings in ascending order. It ultimately doesn't matter whether a function has the same name as another because they are all compared by pointer value internally. It just makes disassembly look nicer.

## Extra Details

Honestly there's nothing much else to add for this functionality. `TOKEN_FUN` now has a zero-binding-power prefix rule that leads to a very barren pass to `function()`:

```c
static void lambda(bool canAssign){
    function(TYPE_LAMBDA);
}
```

Since function declarations parse earlier than this, it is correctly shadowed by `functionDeclaration()`, which *does* require a name identifier.

We add a new function type `TYPE_LAMBDA`, which acts as a flag to tell the compiler to get a random name, and to allow the body to be parsed as a single expression. An `OP_RETURN` is emitted if it is an expression body so it is equivalent to `return expression`:

```c
// in initCompiler():

    if (type == TYPE_LAMBDA){
        current->function->name = lambdaString();
    } else if (type != TYPE_SCRIPT) {
        // get function name
    }

    // ...

    if (type == TYPE_LAMBDA && !isStatement()){
        expression();
        if (match(TOKEN_RIGHT_BRACE)){
            // end of expression.
            emitByte(OP_RETURN);
        } else {
            consume(TOKEN_SEMICOLON,  "Expect ';' after expression.");
            emitByte(OP_POP);
            block();
        }
    } else {
        block();
    }
```

`isStatement` is a static inline helper function for a switch-table lookup; we return true if any keyword that could start a statement is found in `parser.current` (such as `TOKEN_VAR`, `TOKEN_IF`, `TOKEN_FUN` etc.) and false otherwise. This all but ensures the body is either an expression or an expression *statement* (terminated with `TOKEN_SEMICOLON`).

Aaaaaaand we done!