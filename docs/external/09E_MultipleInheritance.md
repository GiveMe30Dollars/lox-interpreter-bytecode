# 09E: Multiple Inheritance

The experimental branch `containers` supports multiple inheritance. A class may inherit from multiple base classes via use of an array literal.

```
class BaseA {/*...*/}
class BaseB {/*...*/}

class Derived < [BaseA, BaseB] {
    // Implementation details...
}
```

The array must be a flat array of identifiers, with each identifier corresponding to a defined class. The Base classes are ordered in descending priority for copy-down inheritance of methods. `BaseA`'s methods will always override `BaseB`'s by default, even if the method itself is copied down from an ancestor and not directly implemented.

The `super` keyword must be disambugated with subscript notation when used with multiple inheritance.

```
class Derived < [BaseA, BaseB]{
    method(){
        super[0].method();          // method in class BaseA
        super[1].method();          // method in class BaseB
    }
}
```

Refer to Document [13I](../internal/13I_MultipleInheritance.md) for implementation details.

The branch `metaclass` has a more permissive implementation where any expression that evaluates to either a class or an array of classes can be used as a target for inheritance.