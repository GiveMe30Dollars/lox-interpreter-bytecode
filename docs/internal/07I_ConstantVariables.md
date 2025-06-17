# 07I: Constant Variables

Big question: should global variables be allowed to be declared constant?

There are compelling arguments in favour and against this:

Yes: 
- Most languages that supports `const` ([not looking at you, Python](https://stackoverflow.com/questions/17291791/why-no-const-in-python)) supports global constants. *No other language I know of makes this distinction.*

No:
- Extra data structures required to store that flag.
- Interacts poorly with the REPL
  - Though `reset` is an option...
  - Then again we do allow global variable redeclaration *because* of REPL considerations.
    - Also what happens if a global variable is redeclared from a `const` to a non-`const`? *Can you do that?*

My rationale is this: It's clear that the Lox REPL is designed to be as permissive as possible. Global variables can be redeclared, type modifiers shouldn't affect this too. Lox *programs* should gain some benefit from type restrictions, otherwise there would be no point at all to adding `const`. And, most crucially, the Lox REPL and Lox programs should share the same ruleset. Code copied from one should function when pasted into the other.

*Aside: You might have noticed that the terminal commands `exit` and `reset` completely bypass this. These aren't useful commands in a normal Lox file, but for the sake of consistency I plan to turn them into native functions.*

Hence, I opted to implement constant variable checking for only local variables. Specifying `const` for a global variable is considered a compile error.

## New Field Who Dis

The compiler Local and Upvalue structs now have one new field:

```
typedef struct {
    Token name;
    int depth;
    bool isCaptured;
    bool isConst;       // <---
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
    bool isConst;       // <---
} Upvalue;
```

I could just only store this in only the Local struct, but then I'll have to walk the whole upvalue path to determine whether something is constant, and frankly we're not hurting for space. Padding is going to eat up any minor space optimizations anyways.

`varDeclaration` takes an additional parameter `Token* constIdentifier`. By default, `NULL` is passed here, but if we scan a `const` keyword we keep it alive as a local object and pass its address along.

`constIdentifier` is reported as an error if we get all the way to a `parseVariable` and we find out we're parsing a global variable:

```
// in parseVariable()
    if (current->scopeDepth > 0) return 0;
    if (constIdentifier != NULL)
        errorAt(constIdentifier, "Cannot declare constant global variable.");
```

Past that, we thread the boolean value `constIdentifier != NULL` all the way to `addLocal`, where it is used to set the `isConst` field in the Local stack.

Similarly, when we call `addUpvalue`, take in `isConst` from either the chained upvalue or the local variable the upvalue points to, and assign that too.

Do all that, and... the compiler will throw an error if we declare a constant global variable.  
Sweet!

## Compile-Time Checking

The reason we even added this.

When we resolve a named variable to either local, or an upvalue, we also keep a corresponding `Local*` and `Upvalue*` pointer. Then, when we find a set operation, we do a check to ensure that `isConst` is not true. If it is, throw a compile-time error at the assignment operator. This is usually `=`, but in my case I also have compound assignments `+=`, `-=`, `*=` and `/=`.

And with that, it is done.

## ...Was It Worth It?

We just made the compiler *slower*, and what for it?

The "compromise" we made with only checking local variables is that it doesn't fundamentally solve the problem that `const` tries to solve: to stop someone from assigning to or *overwriting* something important. That doesn't typically happen with local variables due to how shadowing works, but it *absolutely does* for global variables, especially if said global variable is living in another file.

Lox has no modularity story. My naive approach to `import` and STL functions would just be to dump the contents of the file into the scope you called `import` in. It is *absolutely possible* to overwrite a native function and lose access to it for the rest of the program's lifetime. The `const` keyword doesn't solve this *because* I didn't let it apply to global variables.

Fields can still be written and rewritten. Changing *that* would be a breaking change to Lox. I am not ready to do that.

Honestly, important things like `Math.pi` would probably be written as static property getters in their parent classes such as `Math`. You *could* overwrite the keyword Math, and that *would* cause issues, but you can't overwrite `Math.pi` and break the module across your entire VM. Since Lox classes are closed, once you define a method, that's it. No alterations.

Python does this. It even keeps the original `Math` module around so you can access it if you delete the shadowing variable.

I think the takeaway is to look into how modularity works.

All the work around this has been splintered off into the branch `const-keyword`. I may or may not merge this into the master branch. Time will tell.