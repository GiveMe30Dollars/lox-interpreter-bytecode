# 12I: Static Methods

This one should be a walk in the park after the previous two entries.

If this seems a little backwards, it is! I should've implemented this first oh my gods

## Semantics Squabbling

The `static` keyword means a lot of things and nothing at the same time. It's a term that is dreamt up by programmers, and thus is vulnerable to [XKCD 927](https://xkcd.com/927/).

Still, though, let's try to find out what the general zeitgeist is, so that we can define what it means in *Lox*.

- In C, `static` variables and functions are initialized *once* upon starting the program. They keep this allocation until the program exits. During this time, the static variable or function keeps its value throughout the program.
  - So, for instance, a `static int` in a function will [retain its value between invocations](https://stackoverflow.com/questions/572547/what-does-static-mean-in-c). This helps track state behaviour (handy for a language with no classes) but makes the code less thread-safe.
  - Additionally, static variables have internal linkage, meaning they cannot be seen outside of its source file.
- In C++, they act identically to C. Additionally, a static method in a class can be accessed with scope resolution (that's what `Class::function` actually does!)
- In Java, `static` is an access modifier that only makes sense in classes: it allows the method to be called without creating an instance. Same for JavaScript and C#.

Those are the main ones. As is usual, the distinction usually comes from whether the design approach takes more inspiration from Java or C.

Lox is a fairly high-level language. I think the Java definition works better here.

## More Hash Tables

Nystrom has used [metaclasses](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter12_classes.md) to implement static methods in jlox. This is a pretty neat solution, all things considered!

Some minor concerns:

- C doesn't support inheritance in this way. We basically either have to rewrite all existing Lox classes to fit with Lox instances, or have Lox instances carry methods. Neither option seems particularly appealing.
- This plays *really poorly* with the single-pass compiler. A static method can be defined anywhere in the class body. Where do we put the metaclass on the stack? How does it interact with superclasses? *How are we supposed to interact with it at all? Define new opcodes?*
- You're not sending me to Metaclass Hell that easily.

For these reasons, I just made a new field `HashTable statics` in the `ObjClass` struct. It gets initialized, torn down, and traced through whenever the ObjClass that holds it does so too.

A new opcode `OP_STATIC_METHOD` is added to add a function into this hash table.

## [`super` - shy](https://youtu.be/ArmDp-zijuc?t=40)

*Aside: I'm not sorry.*

Disallowing `this` in static methods is a no-brainer. Static methods can be called or invoked from the class itself, so there is no instance for `this` to bind to.

`super` is trickier. The semantics of `OP_GET_SUPER` and `OP_SUPER_INVOKE` bind the method from the superclass to `this` instance, so disallowing this from use in static methods also makes sense. It's no big loss, since the static method can be accessed using the class name itself.

What is a bit tricky is accessing a static method via `super` from a non-static method:

```
class Base {
    static pi() {
        return 3.1415;
    }
}
class Derived < Base {
    showPiFunction() {
        print super.pi;
    }
}
Derived().showPiFunction();
```

There's no way to check for this during compile-time. At runtime, *this* happens:

```
Unidentified property 'pi'.
[line 8] in <fn showPiFunction>
[line 11] in <script>
```

I think we can all agree this is nonsense. What do you mean "unidentified property"? *It's right there!*

To fix this, `pi` can't just be accessible from its class. It also has to be acessible through a derived class's *instance*.

## Static Methods are Also Non-Static Methods (??)

Yeah. When we add a static method via `OP_STATIC_METHOD`, we also add it to the methods table. This is also a design choice most would consider... questionable.

Just to take stock again and see what most language contemporaries are doing:

- C does not have inheritance, or classes. Zilch.
- In C#, static methods cannot be called like member functions.
- In C++ and Java, static methods *can* be called like member functions.
- In JavaScript, static methods can be called from member functions using `this.constructor.` ([though the semantics from `Class.` varies due to dynamic dispatch](https://stackoverflow.com/questions/28627908/call-static-methods-from-regular-es6-class-methods))
- Python and Rust flips us off and says *all* methods are static. `instance.method(args)` desugars to `method(instance, args)`, and this is reflected in the signature of the method declaration.
- Golang doesn't even *have* static methods.

There's nothing inherently unsafe with accessing a static method through an instance. Since `this` is disallowed in static methods, we can't modify the caller even if we wanted to. It doesn't make a difference whether the static method is called via the class or an instance. That's the whole point, isn't it?

It's bad practice, for sure, but I'll allow it.

## Initializers...

```
class E {}
var a = E();
var b = E.init();
```

What should this code do? They're the same, right?

As of now, no. E has no static method named `init`. In fact, E has no methods at all.

Calling a function to create an instance is a two-step process. First, the VM creates an `ObjInstance`. Then, if a user-defined `init` method is found, it executes the contents of that, bounded to the fresh instance. Which should sound a lot like `instance.init()`, because it is. It's the behaviour you get if you invoke `init` on an existing instance.

As of writing, I'm not addressing this, but it makes intuitive sense to just overload the `init` method with separate static and non-static definitions, even if `init` isn't user-defined. `init` is basically a contextual keyword in this situation anyways.