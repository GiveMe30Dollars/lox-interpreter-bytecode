# 06I: Testing the GC

This write-up assumes you just finished [Chapter 26 of Crafting Interpreters](https://craftinginterpreters.com/garbage-collection.html#when-to-collect), and no further.

Okay, so you just finished up your shiny new [garbage collector](https://craftinginterpreters.com/garbage-collection.html#when-to-collect). Hooray!

How are you going to test it?

No really. Think about it.

Keep in mind that the current types of heap-allocated objects are the following:

```c
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE
} ObjType;
```

Functions and string literals are created during compile-time. They live in the constants array of `<script>` or whatever function they're scoped in. *They'll never be deallocated.* If you stuffed native functions into the globals hash table, they will *also* not be deallocated.

I know that in just another chapter we'll be looking at classes and instances, which definitely can go out of scope and be garbage collected. However, a majority of the objects we have now are eternal: either scoped to the function they're encapsulated in, or in top-level code.

There are two major exceptions to this:  
- Dynamically-created strings: string concatenation (`a + b`) or string interpolation (`"string: ${var}"`) creates new string objects at runtime, and thus can trigger a GC.
- Closures and upvalues: `OP_CLOSURE` creates a closure object during runtime, which encapsulates a function object that is created during compile-time. Upvalues are also created and closed during runtime.

I created [`memtest.lox`](../../tests/memtest.lox) for these types specifically, assuming that the VM flag `DEBUG_STRESS_GC` is enabled.  

I used this little line whenever I wanted to manually trigger a reallocation (and subsequent garbage collection):

```kotlin
var b = "update"
print "${b}: trigger GC!";
```

This is as much a documentation for me as it is for you (the hypothetical reader). I had to write this out to make sure I wasn't making any logical leaps in reading the `stdout` of the garbage collection logs. Here goes.

## Dynamically-Allocated Strings

```kotlin
var a = "first value";
var b = "update";

{
    var b = "${a}: dynamically-created string.";
}

{
    print "${b}: trigger GC!";
}
```

Let's ignore the GC filling the logs when we initialize the VM. Those are native functions, and there is little reason to deallocate a native function if the point is to have them accessible as the interface between Lox and C.  
*Aside:* That is, unless, you overwrite the global identifiers with your own functions. Then the `ObjNative` structs would be inaccessible and be deallocated.

What we care about is when the VM reaches the execution of this line:

```kotlin
    var b = "${a}: dynamically-created string.";
```

This creates a local variable that is assigned the value of a dynamically-allocated string, made using interpolation.

Then, when we reach the GC-trigerring line:

```kotlin
    print "${b}: trigger GC!";
```

It sweeps away the previous dynamically-created string.

![GC1](https://github.com/user-attachments/assets/b1dd0231-4bbf-4588-9218-7f9daff1353e)

Note that because the trigger line works by creating an inaccessible dynamically-allocated string, it will be sweeped when the GC is next called.

![GC2](https://github.com/user-attachments/assets/9d870553-c246-4657-bcca-61ede27fda8a)

# Closures and Upvalues

I had to reread my logs to make sure I wasn't going insane. Turns out I was just misunderstanding scope.

```c++
for (var i = 0; i < 5; i += 1){
    {
        var mult = fun(n) {i * n};
        print mult(2);
    }
    print "${b}: trigger GC!";
}
```

Recall from Document [04I](04I_LoopVariableClosure.md) that the variable `i` accessible in the loop body is an inner variable, shadowing the loop variable that is created in a separate scope.

1. When we create the closure object, we allocate one `ObjClosure` and one `ObjUpvalue`.  
2. When we manually trigger GC, the `ObjClosure` is out of scope, but *not* the `ObjUpvalue`. It is an open upvalue that is *still* on the stack because the inner variable of the for loop is still accessible. The closure object is deallocated. The trigger string is allocated.  
3. In the next iteration, when we create the closure object, deallocate the orphaned closed upvalue and trigger string.

![GC3](https://github.com/user-attachments/assets/179da8fe-9eee-49e4-95b7-692c9fb46e3b)

# What Others?

I could think of a few other edge cases to test:

- Deallocate a function object whose identifier has been overwritten by a new function object, including its associated constants.

This is kinda redundant though. If the GC doesn't work past the next chapter, we'll *know* lol
