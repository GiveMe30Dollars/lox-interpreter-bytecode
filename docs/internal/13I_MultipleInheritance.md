# 13I: Multiple Inheritance

The reason I needed container classes (`ObjArray` specifically and no others) in Sulfox. I'm not sure if this approach is clever or dumb. You be the judge of that.

## The Syntax

Multiple inheritance can be defined using an array literal of identifiers:

```c++
class Animal {}
class Flying {
    fly() {
        print "Flapping wings...";
    }
}
class Mammal < Animal {
    thermoregulate(){
        print "Internal body temperature is constant.";
    }
}

class Bird < [Flying, Mammal] {
    // will inherit fly and thermoregulate
}
```

This wasn't too hard, though I did have to ponder whether I would let *any expression* be a superclass:

```c++
class Derived < getBase(params) {
    // ...
}
var arr = [BaseA, BaseB]
class Derived < arr {
    // ...
}
```

Making this call wasn't easy. Since classes are created during runtime, I *could*, but then it becomes unclear *when* getBase() is called. When the class is declared? Or when an instance is created? *Should* I even let this be fine? 

In a similar vein, while you *can* decouple a superclass array from a class declaration, it doesn't *feel* right. Even worse is that if I reuse `OP_INHERIT` for multiple inheritance, there's no way to differentiate this case of multiple inheritance from *single* inheritance during compile-time, which results in a lot of parsing code breaking.

*Aside: Guess how I found this out. That's right! [By writing this goddamn document!](13I_MultipleInheritance.md) **[AAAAAAAAAAAAAAAAAA](https://tvtropes.org/pmwiki/pmwiki.php/Main/SelfDemonstratingArticle)***

Ultimately, I decided to prohibit this behaviour in the main implementation in branch `containers`. The separate branch `metaclass` holds an alternate implementation where this is permissible.

In the main implementation, another opcode `OP_INHERIT_MULTIPLE` is added. In the `metaclass` implementation, `OP_INHERIT` is overloaded to accept arrays of classes.

Nested array and nested array literals are not permissible. Attempting to do so would throw either a compile-time or runtime error, depending on the implementation.


## The Diamond Problem

[Look, it even has its own Wikipedia entry!](https://en.wikipedia.org/wiki/Multiple_inheritance#The_diamond_problem)

Different implementations do different things. Notably, Rust doesn't have multiple inheritance at all, [opting to utilize its trait system instead](https://doc.rust-lang.org/book/ch10-02-traits.html), which [requires additional disambugation if the names of two trait methods collide](doc.rust-lang.org/rust-by-example/trait/disambiguating.html).

Ultimately (at least for now), I took the approach that is easiest to implement.

As CLox uses copy-down inheritance, the position in which the superclass is positioned in the array affects the order in which its methods are copied down. I implemented it such that the array is traversed from the last element to the first.

Therefore, this inheritance chain —

```c++
class A {
    method() {
        // Implementation details...
    }
}
class B < A {}
class C < A {}
class D < [B, C] {}
D().method()
```
— will call B's version of `method` by default, even if class B doesn't explicitly implement `method` (and thus uses A's implementation) and C *does*.

*No language does this.* Changing it to be something like `D -> (B -> C) -> A` would probably warrant a rewrite of the copy-down inheritance system or `ObjClass`, though, so it's left as-is for now.

## Disambugating `super`

We *do* want the `super` keyword to be resolved correctly, though.

For single inheritance, `super` is a local variable storing the superclass object, which is closed over whenever a method accesses it as an upvalue.

For multiple inheritance, `super` is an array. Hence, we index into it to get the superclass we want. I enforce the usage of subscript notation here rather than the Array `get` method to disambugate from the method we're trying to access.

If we know that the class uses multiple inheritance at runtime, we can enforce this during compile-time. Otherwise, the runtime has to deal with the type-checking. For this reason, the semantics on whether a bad `super` access/invocation (and also bad inheritance) is a compile error or runtime error depends on the branch: compile-time for `containers`, runtime for `metaclass`.