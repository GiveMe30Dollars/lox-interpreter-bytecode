# 10I: Sentinel Classes

***Disclaimer:*** The following Documents [11I](11I_Arrays.md), [12I](12I_StaticMethods.md) and [13I](13I_MultipleInheritance.md) are interconnected with this document. Certain things may be explained out of order. You have beed warned.

Where do I begin?

This is wholly dreamt up by yours truly. It's probably not an *original* idea, but I got there first and it seemed like an acceptable solution.

Object-oriented programming is cool. Everything being treated as an object... less so. The overhead of storing fields and methods usually craters performance, and has lead to such meme-worthy images such as:

<p align="center">
<img src=https://github.com/user-attachments/assets/1e4fd45d-6c5a-4271-a617-a4ecdb352fd1 />
</p>

Usually we don't even *want* an object to store fields. An `int` shouldn't store anything other than its data, and in the case of dynamically-typed languages, its type.

Speaking of... How are we going to do type checking?

This was what I came up with.

## Are They Classes? Kind of. Not Really.

Sentinel classes are artifically-constructed classes, with native functions as initializers and methods. The initializer is basically a way to cast from one data type to another (since the functions are not end-user-defined, the enforcement to only return `this` does not apply), and the methods... well, anything, really. Native functions, man. Go wild.

The power of sentinel classes is that I can implement Lox-accessible methods for objects that must have some sort of back-end support on the C end of the VM: constant lookup for contiguous arrays, subscript notation for container types and such. At the same time, it helps me consolidate all the native functions I intend to implement into categorizable classes (putting `concatenate` as a static method of `String`, for instance).

As for type-checking, that's still a work-in-progress. The native function `type` takes in any object and returns its respective sentinel class (or for actual Lox instances, its class). A class returns itself.  
*Aside: Python's metaclasses are great and also scare me. Let's... not.* 

As for how this interacts with inheritance... it currently doesn't. While `super` can be captured in a method closure it isn't *always* captured. Perhaps a hidden uncallable function would help me with that. That's for future-me to worry about.

There's still some practical considerations to implementing sentinel classes though, namely:

## Lox Has No Modularity Story

Dumping the STL into the globals hash table is a *terrible* idea.

It works fine-enough until somewhere in your program you overwrite it:

```c
var clock = "Time Will Tell";
// Hey I think the clock() native function is gone forever
```

And even if you introduce semantics to delete a global variable in Lox, *you're still not getting your native functions back.*

Most languages explicitly disallow overwriting or shadowing the STL. Somewhere they implemented a functional no-nonsense `const` keyword and from there it's relatively safe to dump it into the global namespace.

Python is an exception.

You can shadow an STL feature, but not *overwrite* it. They're still there and are accessible even when they are shadowed, and when the shadowing variable is deleted the STL reasserts itself:

```python
>>> bool = "Not a callable"
>>> type(bool)
<class 'str'>
>>> del bool
>>> type(bool)
<class 'type'>
>>> bool(1)
True
```

So the STL exists in a larger scope, enclosing the global scope. *Got it.* An extra hash table. Nothing too bad.  
When we resolve global scope we also search here if it is not found.

Naturally, we'll also want an opcode for accessing elements in STL scope. We wouldn't want string interpolation to break because someone shadowed `concatenate`. That's what the new opcode `OP_GET_STL` does.

## Native Runtime Exceptions

Not all function calls are sound. Someone passes an entire array into `Number()`. You cannot tell when this happens until runtime.

How does a Lox native function signal that it has failed?

Nystrom has given his answer in [the original solution to Challenge 24-2](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter24_calls/2.md): return a boolean value, and write the successful result or failure payload directly into the stack.

Cool, except this makes writing a Lox native function a tad bit unintuitive, *and* I sometimes call native functions in the program itself. What then?

I already have a special sentinel value from back when we extended the hash table to take `Value` keys: `<empty>`. This value is unobtainable in Lox, and in fact allowing access to it would be a *bad idea* and would break hash table implementation. There is *no* situation where a successful native function call returns empty. The perfect failure flag.

At first, I wanted to designate `Exception` as the failure object, but there *are* situations where a user might want to create their own Exception instances with try-throw-catch control flow. We can still have native functions return Exceptions for failure: just stash it into the stack!

This makes failure payloads optional for Lox native functions. This also means that I won't get null-pointer dereferences when calling a native function with Values outside of the stack. If a Lox native function doesn't stash a payload, the VM will still helpfully point to the function as the point of failure (since its in position `args[-1]`).

Currently, `type` is the only native function to not stash a payload for failure. This is because I use it in the call logic of the VM itself. In its full implementation, `type` should *never* fail, as all obtainable objects are resolved to their classes.

## Native Methods

Cool, what about native methods?

If the only way a native function could be called is through instance invocation, then slot `args[-1]` is the instance `this`. Perfect.

## Sentinel Invocations

For now, let's just deal with `OP_INVOKE`.

When we call `invoke()`, we first check whether the object is an instance. If yes, we search in its fields, followed by its methods.  
Otherwise? This is where the sentinel classes come into play.

We call `type` to get a sentinel class for this Value, failing immediately if we don't get one. Then, we `invokeFromClass`. That's it. The machinery for this was already in place. We just had to link it up.

Similar logic handling is added for `OP_GET_PROPERTY`, since Lox supports getting bound methods.

## The Import Interface

This is the part most subject to change. It's kind of a mess.

C doesn't like arbitrary nesting without manual allocation or some level of pointer indirection. That means I cannot simply encapsulate a sentinel's methods in its import structure.

So I improvised.

```c
typedef enum {
    IMPORT_NATIVE,
    IMPORT_SENTINEL,
    IMPORT_STATIC
} ImportHeader;

typedef struct {
    const char* name;
    NativeFn function;
    int arity;
} ImportNative;

typedef struct {
    const char* name;
    const int numOfMethods;
} ImportSentinel;

typedef struct {
    ImportHeader header;
    union {
        ImportNative native;
        ImportSentinel sentinel;
    } as;
} ImportStruct;
```

This might look familiar as the tagged-union representation of Values in Lox. The concepts are pretty similar, with the convenience preprocessor macros to boot.

Note that the sentinel struct stores the number of methods in it. This assumes a sentinel's methods immediately follow a sentinel declaration, and any mismatch would either result in a segfault or methods and/or functions being encapsulated in the wrong sentinel.

The `IMPORT_STATIC` enum doesn't have a corresponding struct. This is because they are treated identically to native methods up until inserting it into a hash table, where it also goes into the class's `statics` hash table.

An `ImportStruct` flat array is dynamically allocated and deallocated upon initializing the VM to insert these into the VM's STL hash table.

## Inheriting Sentinels

Disclaimer: it's not pretty.

Inheriting from a sentinel is a bad idea, period. Calling `super.init` would degrade the instance to a primitive data type, and doing anything with its fields afterwards would segfault. Using any of the sentinel methods on a Lox instance would also segfault.

I'll probably prohibit this during runtime. As of now, though, nothing is stopping you from doing it, but its not a good idea. Even for something like `Exception` (or should I call it BaseException?).