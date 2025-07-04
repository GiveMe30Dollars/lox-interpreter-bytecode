# 15I: STL Interfacing Between Sulfox and C

I'm beginning to realise that writing an STL is ***hard.***

I won't sugarcoat it: this is probably one of the hackier things I did for Sulfox, and it's... serviceable? I wouldn't expect *actual* language to be structured like this (I'd probably be horrified if they did), but for a first interpreter this is the best I can muster.

## Native Functions are Kinda Limited, Actually

The biggest problem with native functions is that they can only call other native functions. There is no mechanism for calling a non-native function *inside* of a native function. The VM simply doesn't support it. It is expected that a native function is self-contained.

Using the VM stack in a native function is also a pain in the ass. You *could* do it, but they'll mess up the VM if you forget to pop them before you exit. We could just *not* use the stack... except then the GC will be hounding us whenever we do anything with heap-allocated objects. So anything that is not a number or `true`, `false`, `nil`. Slot `args[-1]` is always available, using the argument slots themselves is questionable, and anything beyond is insanity.

Consider how someone would implement `Array.map(fn)`: for every element in the array instance, call the user-defined function `fn` and write the result to a corresponding result array. If we were to implement this as a native method, it wouldn't be possible. Even if it was, managing the stack would be a major sore point.

*Aside: Python implements `map` as a general function for any object implementing `Iterable`. This should've been the way to go to be honest, though whether the Array iterator needs to be native is... subjective.*

Native functions are (nearly always) *viral*. If a native function has implementation details that require the use of an object, that object would usually need to be native too. For example, a `Slice` object (that's what enables Python's array slicing) has three fields, and no additional methods. It could *easily* be a Lox class. Yet, because it's used in the Array's `get` method, it would need to be implemented as a synth class corresponding to a primitive. Another `Obj` type that also needs boilerplate code for initializing, freeing and GC tracing.

This is annoying. A lot of the time the tedious parts end up being manually managing the VM, because it can't do so itself when a native function is called.

Not all of these problems can be solved. Here are the mitigations I found for some of them.

## Native Functions (and Opcodes) to (One) Non-Native Function

A native function can rearrange its slots to match the call signature of a non-native method. Then, it call's `callValue` or `invoke` for either a non-native function call or a non-native method invocation. Then, it returns the flag `<empty>`.

The flag used to unconditionally signal a runtime exception, in which case `throwValue` is called. Now, we cache the call frame count before calling the native method. If the call stack changed *and* the flag is returned, we treat it as a native function *forfeiting* control to a non-native function. We return success on the call. `run` will load the new call frame in and we continue execution from there. It's basically a tail call (though Sulfox currently doesn't have that).

For this reason, we can only forfeit control to one native function. This is what I do to overload the binary operators for non-numbers, as well as the `OP_PRINT` instruction and `String()` constructor, which instead calls an object's `toString()` method if it has one. In the case of `OP_PRINT`, we can roll back the instruction pointer so that the resultant string is *then* printed.

Of course, this would be naturally slower than just natively implementing something using `vsnprintf`, but with something like nested instances, all with varying implementations of `toString`, this is about the only way to do it without string interpolation breaking for complex data types.

## Does this Method Exist?

`hasMethodNative` returns an unbound function or closure if a method with the identifier name given by `args[1]` is found in instance or data type `args[0]`.

This is mainly used in `OP_PRINT` to check for the existence of a `toString` method, which is optionally called if it does exist. The binary operators do not do this as not defining the corresponding method makes it a runtime error to use said operator.

## Prying Open Synth Classes

`stl.lox` is a Sulfox file that contains method implementations for the synth classes, particularly for Arrays as of writing.

*Update: after christening my variant as Sulfox, you might question why I didn't change the file extension name as well. The answer is that it's not important to me, Sulfox is a superset of Lox, and the only person currently writing Sulfox is also the person writing the Sulfox compiler.*

Yes, you aren't supposed to be able to pry open a class after defining it (which we do by loading native methods into it during the native STL import). I modified the VM runtime to allow this.

`run` now takes in a boolean value `isSTL`, which is always false except for running the `stl.lox` file, directly after the native imports.

This flag changes the behaviour of the VM in several ways:

- Getting and setting global variables reads and writes to `vm.stl` instead of `vm.globals`.
- If the identifier of an existing synth class is used in a class declaration, we fetch the existing class object rather than defining a new one.

This allows me to add synth methods that would be otherwise painful to do, such as those higher-order array methods.