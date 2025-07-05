# 18I: Data Classes

We added Hashmaps into Sulfox. Yay!

Tiny problem: *any value can be used as a key.*  
This is bad for numerous reasons.

## Equality != Identity

Currently, equality is not an overload-able operator. `OP_EQUAL` directly calls `valuesEqual` for the top two objects on the stack, which implements the following semantics for equality:
- For primitives, compare by value. Numbers are casted to `double` so that `NaN != NaN` following the IEEE 754 standard.
- For heap-allocated objects, compare by identity.

For use of hashmap keys, this works out for:
- The singleton values and booleans: `nil`, `true` and `false` are represented as identical values, both in tagged-union representation and NaN-boxing.
- All numbers, barring arithmatic `NaN`.
- Strings. Due to string interning, strings with the same contents refer to the same object in memory, hence identity indicates equality and vice versa.
- Functions, closures and classes, which in most cases *should* be compared by identity; two functions or classes with the same identifiers may not have the same bodies. I'm not sure *why* you would do this, but you can.
- Instances. All instances are unique, and refer to different objects in memory, even though their fields may be identical.

This doesn't work for... literally anything else.

Consider a slice object. It *acts* like an immutable data type, with three values that can be checked for equality. Yet, because it cannot fit on the stack, it is implemented as a heap-allocated object, which is not interned.

```
print Slice(0, 5, 1) == Slice(0, 5, 1);    // false
```

... sorry, what?

Most likely, using a Slice as a key is probably a mistake by a Lox user via erroneous usage of slicing notation, which should be disallowed. *Yet*, this equality comparison is nonsense. It should not behave like that.

Lox interns strings because they are commonly used in the VM for most of anything with interacting with the external environment; IO, reading and writing to files, parsing plaintext-based formats such as JSON. Not interning strings is more of a runtime cost than doing so, especially since Lox does not have a primitive type for characters.

Slices are usually only used in slicing notation, and usually only one will be accessible in the VM at any time. They are not worth interning. However, they should still be compared by value.


## Side Tangent: Comparing Functions, Closures and Classes by Value
  
Comparing functions, closures and classes by identity does mean that there is currently no way to compare functions or classes by value, leading to such gems:

```js
print fun(x){x * 2} == fun(x){x * 2};    // false
print fun(x){x * 2} == fun(y){y * 2};    // false
```

Of course, you can define these lambdas once and use them throughout runtime, though this does break down when talking about modularity stories where function definitions and calls are split across multiple files.

I feel like adding value equality comparisons for functions may lead to further jank. It's intuitive to compare the bytecode contents, but two functions which implement the same operation but with differing details compile to different bytecode representations:

```c++
fun double1(n){
    return n * 2;
}
/*
---- BYTECODE ----
0000 OP_GET_LOCAL 0
0002 OP_CONSTANT  0 '2'
0004 OP_MULTIPLY
0005 OP_RETURN
*/

fun double2(n){
    return 2 * n;
}
/*
---- BYTECODE ----
0000 OP_CONSTANT  0 '2'
0002 OP_GET_LOCAL 0
0004 OP_MULTIPLY
0005 OP_RETURN
*/
```

Resolving this is non-trivial, would require the compiler to compile to AST to be optimized or rearranged by an optimizing step before emitting bytecode, and is honestly more work than I signed up for.

I don't think I'll make function compare by equality by default. You're doing some seriously jank stuff if you're using functions as hashmap keys, and you can work around this by interning your functions, which begs the question of why you're even using functions as keys to begin with.

In theory, it is possible to implement a (rather strict) version of value equality for functions by comparing both bytecode contents.

If the function is a closure, in addition to bytecode checking, check if the upvalues refer to the same variable by traversing each `ObjUpvalue` stored by the functions and checking if they refer to the same variable, closed or open. This can be done by comparing the `ObjUpvalue` objects by identity, since the VM reuses them.

For classes, simply check all methods and static methods and whether each has a corresponding match with the same identifier and bytecode contents. This takes `O(n^2)` time.

This... does feel like encouraging antipatterns though. I don't think this should be implemented.

## Mutable Data Structures Should Not Be Hash Keys

... where do I begin?

```c++
var arr = [0,1,2];
var hashmap = { arr : true };
arr.append(3);

print hashmap.get(arr);
print hashmap.get([0,1,2]);
print hashmap.get([0,1,2,3]);
```

Let's ignore the semantics of hashing and assume we are comparing each entry to the target using something resembling `valuesEqual`.

If we compare objects by value and store a *shallow* copy of it in the hashmap, mutating the variable `arr` changes the key itself to match that of `arr`.  
The first and third `get` operations succeed, the second fails.

If we compare objects by value and store a *deep* copy of it in the hashmap, mutating the variable `arr` does not change the key, which is its old value.  
The first and second `get` operations succeed, the third fails.

If we compare objects by identity, mutation doesn't mean anything. As array literals are desugared to Array constructors, they correspond to newly heap-allocated objects and not the hashmap key.  
The first `get` operation succeeds, the second and third fail.

None of these are reasonable defaults. A user could assume that any of these would work.

Let's now *consider* the semantics of hashing, and how we need to compute a hash to be able to access a value in `O(1)` time.

Assuming that comparison by identity is used, we're fine. We can keep using the pointer value (or a value computed from the pointer value) as a hash.

Assuming that comparison by value is used, a new problem arises in the hashing algorithms and when to call them. Upon mutation? Rehashing also means reinsertion into the hash table, which is bad and expensive and there's no good way to signal that to any hash table that captured this mutable object. If we don't do that, this breaks the invariant that `if a == b, then hash(a) == hash(b)`, which cripples our hash table implementation.

For this reason, Python and Rust disallow use of mutable data structures in hashmap keys. JavaScript and Java do not explicitly specify against it but warns users of the unexpected behaviour this causes. Using mutable data structures as keys is generally perceived as an [unacceptable antipattern](https://stackoverflow.com/questions/1670006/acceptable-types-to-use-as-keys-in-a-hashtable).

This also means that only the singleton values, numbers and strings can be used as data types if we compare by value.

This... isn't great. Some data types like `Complex` can be thought of as new primitive types, and `Complex(5, 3) == Complex(5, 3)` is a reasonable thing people expect. Some other data types, especially mutable ones or those that are constantly instanced, should *not*, and `Asteroid(x, y) != Asteroid(x, y)` should not be considered as the same object as the other even if they are at the same position at this time. That would be a situation where equality must depend on identity.

Gods...

## What Then?

We'll have to make some somewhat-arbitrary design choices for Sulfox. Here goes.

- All object types are mutable and compared by identity unless specified otherwise. For strings, due to string interning, equality and identity are equivalent.

- All accessible object types now internally have a new field called `isLocked`, and the ability to lock them. Locking an object causes all properties to be immutable, and computes a hash for the object. If said properties contains an unlocked object, create a locked deep copy.

- All immutable data structures are locked upon construction. This includes functions, closures and classes, even if they compare by identity regardless.

- A new synth class `Lockable` is defined, which implements the native methods `lock`, `@hash` and `copy` for both Lox instances and synth data types. A sentinel `init` function is also added to cause constructing a raw Lockable to throw a runtime exception. `Lockable` is to be inherited by user-defined classes, and if the method is overriden, `super.lock()` should be invoked in a method body to lock the instance.

- Inbuilt mutable structures implemented Sulfox-side as synths (in this case, Array and Hashmap) inherit from `Lockable` and implement their own `lock` methods, either as native methods or as `stl.lox` Sulfox methods.

- `OP_EQUAL` now directs to a native function. Unlocked objects are compared by identity; locked objects are compared by value. If one object is locked and the other isn't, throw a runtime exception.

- Attempting to insert or search for an unlocked key in a hashmap throws a runtime exception.

Fine. I can work with that.

## Oh Boy More STL-Synth Coupling

The reason the implementation of `lock` and `@hash` needs to account for synth data types is mainly so that container types such as Arrays and Hashmaps can implement locking of their elements before locking of the object itself. This is so that we *do not* need to implement these `lock` methods as native methods. 

Passing control for native methods is painful. I had an entire sub-write-up on this before I realised that it is totally redundant due to the existence of `stl.lox`. And we *will* have to pass control at some point because user-defined `lock` methods should be called first due to overwriting of the copied-down method. Yippeee.

This does make things slightly slower, but fuck it wiring in a way for native methods to temporarily transfer control is worse.

That redundant write-up will be included at the end of this one.

# Bonus: The Redundant Write-Up For Passing Control from Native Functions

## Side Tangent: We Need to Actually Implement Something Resembling Non-Native to Native Function Calling

Arrays and Hashmaps can contain `Lockable` user-defined classes. When we lock an array, if this struct is not locked too, we need to create a deep copy, then lock it. Some user-defined classes will implement `lock` (remember that call to `super.lock()`!), the contents of which will need to be executed.

... goddammit.

Well then.

## Side Tangent: This Could've Been Its Own Write-up

Let me be clear: the following terms have distinct meanings in this section with regards to native function calling logic:
- Forfeiting control: a sort of tail-call, where the 

First off: storing values between native function invocations.

This isn't something I plan to do much of, and the VM's `push` and `pop` functions are exposed to Nativeland, so we can get away with pushing and popping local variables onto and off the stack. Now what?

We implement inaccessible native functions that takes those as arguments.

Second: transferring control.

Callframes now store a `ObjNative*` "return execution to native function" field, which is NULL by default. If we ever have to transfer execution to a native function, we rearrange the stack for the signature of that function and call it using a new VM function, which (in addition to the usual information regarding calling) also takes in a `NativeFn returnFunction`. If we end up calling a native function, we return control after it is called in-place for the stack. If we pop a callframe and this field is set, we call it with the existing stack elements. The task of ensuring that

This does mean a lot of tedium and defining inaccessible native functions for each case where we transfer execution. Luckily, cases like these are few and far between, and most of the STL can either be defined wholly in `stl.lox` or `native.c`.

```c

static Value getNativeFunction(Value* args, int arity, const char* methodName){
    /* helper function implementation for GC dancing. If an error occurs, write to args[-1]. */
    // calls hasMethodNative.
}

Value arrayLockNonNative(int argCount, Value* args){
    // args[0] stores the current index we have progressed to
    // args[1] stores the locked deep copy of the instance that implemented 'lock'
    /* writes the locked deep copy in-place. Then, pops args[1] and forfeits execution to arrayLockHidden */
}
Value arrayLockHidden(int argCount, Value* args){
    // args[0] stores the current index we have progressed to
    // since this is only called from arrayLockNative or itself, we trust that it is an index
    int idx = (int)AS_NUMBER(args[0])

    // We stored a local variable!

    ObjArray* array = AS_ARRAY(args[-1]);

    for (/* */; idx < array->data.count; idx++){
        Value element = array->data.values[i];
        if (/* value is immutable as locked object or primitive */)
            continue;
        Obj* funcLock = getNativeFunction(/* ... */);
        Obj* funcCopy = getNativeFunction(/* ... */);
        if (AS_BOOL(hasMethodNative(/* lock and copy */))){
            // rearrange stack to transfer execution to `lock` and return execution to arrayLockNonNative
            // which can be native or non-native
        }
    }
}
void arrayLockNative(int argCount, Value* args){
    // args[-1] stores the array instance to lock
    push(NUMBER_VAL(0));
    Value result = arrayLockHidden(argCount, args);
    pop();

    // if there is any errors, args[-1] is written to. We propagate the result regardless.
    return result;
}
```
