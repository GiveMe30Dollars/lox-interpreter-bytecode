# 11I: Arrays

This marks the first unique addition to Sulfox since the completion of the book! Rejoice!

Nystrom himself has commented that [arrays are the sole remaining addition to get other container types up and running](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter13_inheritance/3.md), so this is a pretty significant addition to Sulfox.

## Object Representation of Arrays

`ObjArray` is currently a wrapper around the `ValueArray` struct. Ideally, you'd embed this so that there's no unecessary indirection (which does affect performance), but this is currently what's most convenient.

You'll see in a moment that it barely matters. The bulk of array operations interface through native methods, which can be updated afterwards with minimal effort.

## Array Native Methods

`Array` is a synth class, since the runtime representation of arrays is not a Sulfox instance but an `ObjArray`. There are currently four native functions implemented:

- `init`: initializer of arity 1. Returns an empty array of `args[0]` length.
- `@raw`: variadic static method. Returns an array containing all arguments passes to it.
- `get`: method of arity 1. Gets the value stored at the integer index provided.
- `set`: method of arity 2. Sets the value stored at the index corresponding to `args[0]` to the value of `args[1]`.

You might be wondering about that `@raw` method. Don't worry, it'll make sense in a bit. 

## Array Literals

This, too, is a native function call.

We [previously](10I_SentinelClasses.md) added a way to get an STL identifier while bypassing the global variable space. At first, the Array initializer *was* `@raw`, but the semantics of that were ugly. I doubt anyone calls `Array(5)` to create array `[5]` rather than `[nil, nil, nil, nil, nil]`.

To designate a separate function for initializing arrays, I either needed a second STL function, or a static method encapsulated in the `Array` class. I went with the latter.

If this seems a bit circular, that's because it is! Compilers and interpreters tend to have a lot of cyclic dependencies, and Nystrom himself has stated that [printing is an inbuilt statement rather than a function call to break such a circular dependency](https://journal.stuffwithstuff.com/2020/04/05/crafting-crafting-interpreters/).

I'm not as diciplined as Nystrom. I used `Array` as a variadic constructor until I implemented static methods, and then went back and retconned the original constructor to be the `@raw` static method.

With all that said, parsing is easy now. `[` now has a prefix rule that evaluates all of its elements (until it finds a matching `]`), at which point `@raw` is invoked.

The reason why `@raw` has an at-sign in the beginning is simple: I don't want end-users to be able to access this function. The parser does not recognise the at-sign as a valid character, and thus no Sulfox code can actually parse to the identifier `@raw`. We do not have such constraints in the compiler and can hardcode a `@raw` identifier into the parsing logic of functions. This basically functions as an "`OP_BUILD_ARRAY`" opcode, without the opcode. Neat!

## Subscript Notation

Syntactic sugar to `get` and `set` native method calls. 

That's it. 

This will become more of a trend as we go along building the STL. Most of the hard work is done in structuring the VM to import these native methods and sentinels. Once we're there, we can use the existing opcodes for everything else.

The parsing rules around this are very similar to that found in variable getting/setting, as well as function invocations. Since `OP_INVOKE` doesn't require the invoked method to be on the stack but in the constants table, we can parse with minimal lookahead. We assume that the expression is a getter up until we reach `=`, at which point (if `canAssign` is true, of course) we treat it as a setter.

Currently, arrays only support integer indexing. A later addition would be Python-style slices, but that can be accomodated for in the native method implementation.

## Syntactic Sugar for Thought

There is no runtime check for whether a class inherits from a higher class to be able to use subscript notation. All you need to make use of it is to implement `get` and optionally `set` (for immutable types, don't do this lol).

This is... a choice.

I originally wanted to make subscript notation less accessible; much earlier ago I read through [Hamilton's excellent Kotlin superset implementation of Lox, **klox**](https://github.com/mrjameshamilton/klox/tree/master) and while I was (and still am!) amazed with the features added, it also stood out how easy it was to overload the binary operators. It wasn't a design choice I agreed with at the time. I was leaning more towards Python's approach of parent classes / traits such as `Sequence` and `Mapping`. You had to opt into using subscript notation.

Now, though? A runtime check is a runtime check, and something like inheritance where, *at worst*, the entire inheritance chain has to be traversed? That's expensive.

I'll leave it be for now. Maybe I'll splinter off a branch that enforces this check. Who knows. I'll have to implement that native function anyways.