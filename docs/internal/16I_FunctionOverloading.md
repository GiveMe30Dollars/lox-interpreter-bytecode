# 16I: Function Overloading

At first, I thought that this would be wholly unecessary: since dynamic arrays are now first-class objects, we can use an array for any number of arguments. Native functions can also be secretly variadic, though this isn't exposed to the end-user (even if I use it liberally, what with `Array.@raw()` and all that).

What made me end up consider adding it was a confluence of the following:

- I want the Array initializer to be able to take in an iterator and construct an array, in the order that the iterator emits each element. This is so that a method like `.map(fn)` can be implemented via inheritance of the iterator protocol rather than any implementation-specific details of the container class itself.

- Iterators *cannot* be primitive objects. To make `.map(fn)` and `.filter(fn)` implementation-agnostic relative to the container class (other than the `iter` method and the specific iterator for that container type), the special iterators they use would inherit from a normal iterator for the container type but have additional logic for `hasNext` and `next`. For instance, `map`'s and `filter`'s iterator would look something like this:
    ```c++
    fun createMapIterator(container){
        var Iterator = type(container.iter());
        class MapIterator < Iterator {
            init(..., fn){
                super.init(...);
                this.fn = fn;
            }
            hasNext(){
                return super.hasNext();
            }
            next(){
                return this.fn(super.next());             // Return evaluation of function
            }
        }
        return MapIterator;
    }
    ```
    ```c++
    fun createFilterIterator(container){
        var Iterator = type(container.iter());
        class FilterIterator < Iterator {
            init(..., fn){
                super.init(...);
                this.fn = fn;
                this.nextValue = nil;                     // Next value is evaluated ahead of time
                this.exhausted = false;                   // Additional field to track whether exhausted
                this.next();
            }
            hasNext(){
                return this.exhausted;
            }
            next(){
                var value = this.nextValue;
                while (super.hasNext()){
                    var temp = super.next();
                    if (this.fn(temp) == true){
                        // This is the next value that satisfies fn(value)
                        this.nextValue = temp;
                        break;
                    }
                }
                if (super.hasNext() == false and value == this.nextValue)             
                    // Traversed fully, and no subsequent element satisfies fn(value)
                    this.exhausted = true;
                return value;
            }
        }
        return FilterIterator;
    }
    ```

- Therefore, the array initializer has to be non-native. Working with Lox instances outside of the VM in Nativeland is tedious and horrible, and native functions (so far) can only forfeit control to one non-native function as a sort of tail-call. For a potential implementation of an Array constructor using an iterator, it *has* to exhaust it:
    ```c++
    class Array{
        init(iter){
            // Let's not consider the other arguments an Array initializer can accept, yeah?
            while (iter.hasNext())
                this.append(iter.next());
        }
    }
    ```
  Those iterator function calls don't come cheap in Nativeland.

- The array initializer has to take in one argument, and `Array(nil)` is counterintuitive and ugly as sin compared to `Array()`.  
  *Aside: I forgotten that `Array(0)` exists, and that is relatively clear. Uhhhhhh*

Funny thing is, I thought I already know how to do it even before I had implemented it. I was extremely incorrect.

<h1 style="color:red">Big Red Disclaimer</h1>

This disclaimer is added after I typed out all what follows, and *only then* decided to do some reading on existing literature.

Here, I use the term "function overloading" colloqually. According to Wikipedia:

> In some programming languages, function overloading or method overloading is the ability to create multiple functions of the same name with different implementations. Calls to an overloaded function will run a specific implementation of that function appropriate to the context of the call, allowing one function call to perform different tasks depending on context. 
<br>*— [Wikipedia: Function Overloading](https://en.wikipedia.org/wiki/Function_overloading)*

Also according to Wikipedia:

> Multiple dispatch should be distinguished from function overloading, in which static typing information, such as a term's declared or inferred type (or base type in a language with subtyping) is used to determine which of several possibilities will be used at a given call site, and that determination is made at compile or link time (or some other time before program execution starts) and is thereafter invariant for a given deployment or run of the program. Many languages such as C++ offer robust function overloading but do not offer dynamic multiple dispatch (C++ only permits dynamic single dispatch through use of virtual functions).
<br>*— [Wikipedia: Multiple Dispatch](https://en.wikipedia.org/wiki/Multiple_dispatch#Understanding_dispatch)*

Wait, what?

Therein lies the rub: **What I'm referring to when I say "function overloading" is more accurately described as dynamic dispatch.**

I use static-type languages, typically supporting only single dispatch, and personally code in a single-dispatch manner as a hobbyist. For the longest time I thought these two terms were equivalent, and got really confused when the nice folks at StackOverflow and friends claimed [function overloading doesn't make sense for dynamic languages](https://softwareengineering.stackexchange.com/questions/425422/do-all-dynamically-typed-languages-not-support-function-overloading).

My bad. Temper your expectations. I am not an authority on *any* of this.

Thanks, Wikipedia. And *thanks*, Wikipedia.

<p align="center">
<img src=https://github.com/user-attachments/assets/d7e90445-65da-4218-8cd1-5e9a6ff1ffee />
</p>

<h2 style="color:red">Preamble and Setting Expectations</h2>

I'd recommend reading through the Multiple Dispatch Wikipedia article, especially those sections on [Understanding Dispatch](https://en.wikipedia.org/wiki/Multiple_dispatch#Understanding_dispatch) and [Data Types](https://en.wikipedia.org/wiki/Multiple_dispatch#Data_types). More generaly, refer to the topic of polymorphism in programming.

There are a few main components that cause dispatch implementations to differ:

- Whether the language is statically-typed or dynamically-typed.
  - C, C++, Java, Rust, Haskell etc. are statically-typed: They have types such as `int`, `bool`, `float` and so on, and whenever you declare a variable you *must* specify a type. Type-checking is done during compile-time, and typically type representation is diminished or outright stripped during runtime.
  - Python, JavaScript, PHP etc. are dynamically-typed: Variables are either implicitly declared or use a common keyword such as `var` or `let`. Type information is encoded in the data values itself, and type-checking is done during runtime. Dynamically-typed languages may have optional type annotations, which reintroduces compile-time type-checking.

- Signatures: how much information disambugates functions.
  - None / Identifier Only: Defining a function with the same identifier as a previous one is either a compile error, or overwrites the previous definition.
  - Arity: Functions that take in different numbers of arguments are distinct.
  - Type: Functions that take in different argument types are distinct. Usually only available for static-type languages.
  - Return type: Functions that return different types of values are distinct. Usually only available for static-type languages that also specify return types of functions. Usually requires strong typing, in which one data type cannot be implicitly converted to another.

- Static or dynamic dispatch. Not to be confused with static or dynamic *typing*.
  - Static dispatch: resolving function calls occurs completely during compile-time. During runtime, two functions with different signatures are treated as different functions, even if they share the same identifier.
  - Dynamic dispatch: one or more aspects of resolving function calls occurs during runtime. An identifier may refer to several or all functions that share that identifier, despite having different signatures.

- Single, double or multiple dispatch: the amount of information used for resolving and choosing which implementation of a function to run.
  - None: There exists exactly one implementation, there is no choosing.
  - Single dispatch: Implementation is chosen based on one piece of information. This includes methods in object oriented programming; `Cat.sound()` is distinct from `Dog.sound()` due to classes being different. The class itself can be thought of as the 0th argument of a method call, from which an implementation is chosen.
  - Double dispatch: Implementation is chosen based on two pieces of information.
  - Multiple dispatch: Implementation is chosen based on multiple pieces of information.
  - Note that double and multiple dispatch can be imitated using conditional branching, pattern matching or design patterns such as the Visitor pattern.

Under these defintions, it doesn't make sense to talk about function overloading the way C and friends have it: function overloading is done via type signatures, and type information at compile-time is only accessible if the language is statically-typed.

When I talk about overloading with regards to Lox, I'm referring to arity overloading, which is providing different implementations of functions with the same identifier based on the number of arguments it takes. This is available regardless of typing.

```c++
fun overload(){
    print "Arity 0.";
}
fun overload(arg){
    print "Arity 1!";
}
```

Notably, these implementations could be totally distinct. `overload(arg)` could be thought of as its own function in its own right. In practice, arity overloading tends to be used to supplement a main implementation with additional ways to call it, such as by supplying fewer or more arguments, especially if the language does not natively support optional arguments.

In this way, arity overloading can be thought of as single dispatch, with the piece of information used to choose implementations is the number of arguments. As methods, they are double dispatch; both the class type and the arity is used when choosing an implementation.

In a static-dispatch paradigm, these functions *are* totally distinct. C++ is one such instance: all functions are name-mangled during compile-time to reflect their signatures, which are then called. There is no additional type-checking of arguments or resolution during runtime.

Usually, such a paradigm necessitates downgrading functions from first-class objects. C++, C and similar languages such as Java have layers of indirection when it comes to using functions with variables, whether it's function pointers in the C family or using the Runnable interface to create a class, from which you can use an instance to call it as a method. You typically cannot pass a function as an argument to another function.

Lox is not such a program. We do cover this in a later section, but to give a teaser:

```
var myFunction = overload;
myFunction();
myFunction("argument");
```

`myFunction` could refer to either `overload` of arity 0 or 1. Since functions can be passed around, and a function can be separated from its call, we don't know which implementation of `overload` to use until the runtime encounters the infix `(` for a call, and the number of arguments supplied. *It is both.* Lox has to use some form of dynamic dispatch to differentiate the two. This comes at a non-trivial runtime cost.

Most dynamically-typed languages do not have this form of "overloading". The only one that I could find that does is [Wren, which is another one of Bob Nystrom's languages](https://wren.io/functions.html).

To quickly go over the highlights of the big names:

- Python has variadic functions and default parameters but does not allow overloading by default. [Multiple dynamic dispatch is supported in the form of an optional library.](https://stackoverflow.com/questions/6434482/python-function-overloading)
- JavaScript does not have any form of function overloading. TypeScript, which is designed ground-up as a superset of JavaScript and to run in the same VM, does have [compile-time arity, type and return-type signature discrimination but only allows for one implementation body](https://stackoverflow.com/questions/13212625/typescript-function-overloading). The signature of the implementation must be a union of all the signatures declared.
- PHP has [default parameters and variadic functions, but no function overloading](https://stackoverflow.com/questions/4697705/php-function-overloading). Magic functions and methods are not subject to the limitations of the PHP runtime and should not be considered.
- Julia, Haskell, Clojure and other functional languages inheriting from ML uses pattern-matching to achieve static multiple dispatch via type signature.

**With all that said**, *sematically*, Lox implements dynamic single dispatch for functions, and dynamic double dispatch for methods. Global definitions are late-bound, as are all other Lox global variables, and local variables are resolved lexically.

As for implementation details? I'll be resolving most common cases of dispatch statically, during compile-time, similar to how base Lox avoids creating bound methods wherever possible. I later refer to this as "shortcutting". There's some cases where it's feasible, and some where it isn't. Generally, I won't resolve return values across call frames, nor when assigning to a variable, even if only one implementation is called via that variable.

*Aside: I should really use another name that indicates that I've diverged, **far**, from where Crafting Interpreters initially ended, and that this is a superset of Lox. Nothing immediately comes to mind though.*

## Name-Mangling Functions

This part I knew. Nystrom implemented something similar for [BETA-style `inner` method semantics](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter29_superclasses/3.md).

When we define a function, we also store its arity in its identifier signature. Then, when we call a function, we search for a function with that arity. Sounds simple, right?

This format could be anything as long as it represented the arity of a function, appended to its original name. For the sake of consistency, this write-up will use the format `function@0xNN`, where the arity signature is an at-sign (to prevent accessibility by Lox users) followed by a two-digit hexadecimal number. As the maximum number of arguments in a function is 256-exclusive thanks to stack limitations, this covers all possible arities a function could have.

Helper functions for name-mangling and unmangling identifiers for `ObjString`s, extracting the arity of a mangled identifier, and checking whether an identifier represents a mangled name should be defined. Additional fields for `Token` should indicate whether it represents the identifier of a function and its arity. This difference in representation is mostly because Tokens and their fields are not dynamically allocated: they either point into the source string or into string literals, which are eternal in C. Also, I am *not* adding a new identifier type to Lox.

Anonymous functions and lambdas are not name-mangled, since they are compiled once and either remain on the stack as an expression or are assigned to a global variable. Either way, you cannot access them through a function name identifier anyways.

That should be it. Right? *Right?*

## We Cannot Resolve (Some) Function Calls At Compile-Time

Two things cause implementing function overloading in Lox to be particularly painful:
- Functions are first-class objects. They can be passed around and stored in variables.
- A call can be separated from the function object itself. A call is an operation, just like the typical binary operators.

Consider the following:

```c++
fun facTory(){
    // no, I'm not sorry.
    fun fac(n){
        if (n <= 2) return n;
        return n * fac(n-1);
    }
    fun fac(m, n){
        // let's define this as the factorial of the sum.
        return fac(m + n);
    }
    return fac;
}
```

Quick question: what are we returning from `facTory`? It's the `fac` function, sure, but which one? The one with arity 1, or 2?

That's a trick question. **It's both:**

```c++
var myFunction = facTory();
print myFunction(5);                // 60
print myFunction(3,2);              // 60
// This is all valid and if we're not supporting this, why are we here?
```

Also, remember how Lox global variables are late-bound? Yeah that applies to function definitions too:

```c++
var gloablFunc;
{
    fun single(m){
        return printline(m);
    }
    globalFunc = single;
}

// the scope where single is created is ended, but we retain a reference through globalFunc
// the identifier printline is resolved to a global variable, which we define here:
fun printline(m){
    print m;
}

globalFunc("Printed line!");         // Printed line!
```

Which logically leads to:

```c++
var gloablFunc;

fun printline(m){
    print "We're not using this!";
}

{
    fun double(m,n){
        return printline(m,n);
    }
    globalFunc = double;
}

fun printline(m, n){
    print m;
    print n;
}

globalFunc("One", "Two");               // One
                                        // Two
```

This does seem like a questionable choice, I'll admit. We made our bed when we decided global variables should be treated differently from local variables or upvalues. We now commit to them lest Lox becomes more inconsistent.

When we parse `double`, we only have `printline@0x01`. If we resolved the `printline` identifier to that, the runtime would throw an arity exception. Should we do that? Not unless we want to resolve global variables in lexical scope.

All of that is preamble to the main discussion: we need a new way to represent multiple-arity functions (or a collection of multiple function definitions, depending on whether you're behind the Lox curtain).

## Function Bundles

An `ObjBundle` is an object wrapper for a hash table, mapping arity count to the `ObjFunction` or `ObjClosure` it represents. It should appear identically to a function for a Lox end-user, and thus falls under the sentinel class Function.

*Aside: not that there's currently anything there other than a way to check a function's arity. A function constructor that works during runtime is a whole other topic/article due to the reduction of local variables to slot indexes rather than identifiers during runtime. Would probably necessitate turning the sentinel identifiers into reserved keywords, and more compiler-VM coupling.*

A function bundle only contains functions of the same identifier accessible via local variables or upvalues. If a function bundle is called, we disambugate the specific function and call that instead. This is also done for bound method bundles, before it is replaced by `this`.

The static constructor of a function bundle will be passed all its accessible functions. Then, it returns either a new function bundle or `args[0]` for argCount == 1.

## Parsing Function Identifiers

When parsing an identifier, we know a local variable is a component of a function bundle if its corresponding Token is a function identifier.

Once we know this, we know that we are parsing a function identifier, and do the following:
- Emit `OP_CONSTANT` to put the sentinel class `Function` onto the stack.
- Emit the get instructions to push the bundle component found onto the stack.
- Walk through all local variables, including enclosing scopes, and check if the token contents match.
  - If yes, check if the token is a function identifier.
    - If yes, emit the corresponding instructions to get it onto the stack as well.
        - If it is in an enclosing scope to this scope, capture and create an upvalue chain to this scope.
    - If no, skip this scope and continue search in the next.
- Emit `OP_INVOKE` instruction for the static constructor of a bundle.

*Aside: I wonder if function features such as method binding and bundle creation* should *be exposed to the end-user via native functions. I can imagine both great and terrible things arising from it.*

## Shortcutting Function Identifiers

Of course, if we can see the function identifier is called as soon as it is declared, we can omit all of this.

```c++
{
    fun sum(a, b){
        return a + b;
    }
    fun sum(a, b, c, d){
        return a + b + c + d;
    }

    sum(1, 5);
}
```

With one token of lookahead, after we identify the indentifier to be a function identifier, we can see that it is immediately called due to `TOKEN_LEFT_PAREN`. Hence, there is no need to construct the bundle:
- Emit `OP_JUMP`. We'll be parsing the argument list first to get the number of arguments, and this helps to arrange the stack so that the function object is still beneath all of them.
- Save the current chunk count for a later loop emission.
- Parse the argument list. This loads them onto the stack.
- Emit `OP_JUMP` for a jump, this time to skip the fetching of the specific function object.
- Patch the jump past the argument list.
- Create a synthetic token from the contents of the identifier, corresponding to the name-mangled identifier of the function with the arity of the number of arguments.
- Emit the instructions required to load the function variable onto the stack using `namedVariable`. This handles local, upvalue and global variables.
- Emit `OP_LOOP` to the start of the argument list parsing.
- Patch the jump past the function fetch.

It does kind of suck that a normal function call now has three extra jump instructions, but between this and creating a heap-allocated object that is discarded next instruction? This wins out.

*Thoughts:* you could change the semantics of function calling to resemble `OP_INVOKE` or `OP_SUPER_INVOKE` more, or emit an inaccessible sentinel constant like the existing `<empty>` or a new singleton value that accepts such use for normal functions. This saves all the jumping but does require new VM behaviour. This is an acceptable compromise, though, since we'll have to change some VM plumbing regarding function calls and method invocations anyways to accomodate the addition of bundles.

## Runtime Support for Calling Bundles

The runtime now needs to do additional work for a function call. Keep in mind that after the name-mangling and bundling shenanigans, *this* is what the runtime sees:
```c++
fun facTory@0x00(){
    fun fac@0x01(n){
        if (n <= 2) return n;
        return n * fac(n-1);
    }
    fun fac@0x02(m, n){
        return fac(m + n);
    }
    return Function.bundle(fac@0x01, fac@0x02);
}

var myFunction = facTory@0x00();
myFunction(5);
myFunction(2, 3);
```

By the time we get to these function calls, `OP_CALL` and friends will be carry the byte operand necessary to resolve the bundle in `myFunction` to the specific function of either `fac@0x01` or `fac@0x02`.

Simple, then: if the callee is a bundle, we look in its hash table for an arity match, replace the bundle with the specific function stored, and attempt to call that. If we don't find what we're looking for, we create the name-mangled identifier and search in the globals hash table, then the STL table. If nothing is found, throw an undefined exception.

Keep in mind that we still need the runtime arity check before pushing a new frame due to anonymous functions and lambdas, which is redundant work done. I could remove this with some selective optimization, but I don't think it'd be significant enough of a runtime cost to justify optimizing.

## Adding Bundle Support for Methods

Consider the following. For convenience sake, method identifiers are shown name-mangled:

```c++
class Person {
    shout@0x00(){
        print "AAH!";
    }
    shout@0x01(message){
        print message;
    }
}
var Bob = Person();

Bob.shout();
Bob.shout("Look out!");
var voicebox = Bob.shout;
```

The first two shouts are fine. We can resolve method invocations in the same way we resolved immediate calls to their specific functions. That's more repeated work, sure, but it can be abstracted into a common implementation that both an access-and-call and a method invocation can use.  
What is concerning is that third line. The bytecode emitted would be:

```
OP_GET_GLOBAL    'Bob'
OP_GET_PROPERTY  'shout'
OP_DEFINE_GLOBAL 'voicebox'
```

We don't have a property in the Person class for `shout`. The two functions with that identifier have been name-mangled. This would throw an undefined property exception.

Alright, so before we close the class, we need to create these bundles and put them into the class as methods. This can only be done efficiently if the compiler has knowledge of all the methods defined, so we need to add a hash table (or a pointer to a hash table) in the `ClassCompiler` struct, which is allocated and deallocated within the execution of `classDeclaration`. Defining or inheriting a method writes to this table, with keys corresponding to the identifier and values corresponding to the constant index of the function.

Just before we emit the `OP_POP` instruction to knock the class off the stack, we do the following:
- Get the first non-empty entry.
- Unmangle the name and save it somewhere, both as a Token and as a constant in the chunk constants array.
- Emit the opcodes to load sentinel class `Function` and said entry onto the stack.
- Delete that first non-empty entry.
- Iterate sequentially through the rest of the table's entries.
  - If the mangled name corresponds to the saved name, emit the opcodes to push it onto the stack, then delete it from the table.
  - Otherwise, save the index of this entry of a different name. This will become the first entry for the next iteration.
- Emit the `OP_INVOKE` instructions to construct a bundle.
- Emit `OP_METHOD` with the constant index of the unmangled name to assign it as a method of the class.
- Repeat until no more entries are found.

The major upside to this approach is that it works without us changing the semantics of function name-mangling, while keeping the time complexity of method access to constant-time. The major downside is the space: the worst case scenario is also the most common, where methods have one or only several definitions. The methods hash table *doubles* in its number of entries.

Frankly, I don't think there's a good way to do this. It's easy to search for a non-mangled name in a hash table if you have the mangled one, but not vice versa without the arity count. If we try anyways, we get linear time complexity. Trying to look ahead to *only* name-mangle functions with multiple definitions requires more state-tracking by either the ClassCompiler for methods, or the whole compiler for all functions. There's a tradeoff either in space or time, whether that's in the compilation or execution of the source file. It's... a bit of a headache, I won't lie.

## Shadows Over Innsmouth

Consider the following:

```c++
{
    fun fac(){
        return "Nothing.";
    }

    {
        fun fac(n){
            if (n <= 2) return n;
            return n * fac(n-1);
        }

        print fac(5);        // 60
        print fac();         // <----
    }
}
```

Without function overloading, the inner definition would shadow the outer one. Now, it displays "Nothing.".

Should it be like this?

I can see arguments for and against this. Done purposefully, it could be rather powerful. It could also be the gateway to many frustrating logical bugs, especially in conjunction with the late-binding of global functions. You'd be the judge, I suppose.

## Why Are We Doing This?

Do you see why most dynamic languages simply *don't?*

I'll admit that typing this out gave me a lot to chew on, and it does sound exciting, but this is a rather large structural change to how Lox works.

As of writing, I have not implemented this yet. I *want to*, but I also want to add tail calls and array slicing and first-class hash tables, and those are easier *by far*. This feels like branch material.

Keep an eye out here, I suppose. Let's see if I'll end up implementing it.
