# 13I: Hashmaps

The experimental branch `containers` now implements hash tables as first-class objects.  
A hash table can be instantiated using the `Hashmap` constructor or using literal notation, and may be accessed and written to using either `get` and `set` method calls or subscript notation. An additional method `has` returns a boolean value for whether a key is present in the hashmap.

```
var hashmap = Hashmap();
var hashmap = {};

hashmap["one"] = 1;
hashmap["two"] = 2;
print hashmap["one"];         // 1
print hashmap.has("three");   // false
```

Note that as of writing, classes, instances, arrays, slices and hashmaps are compared by identity. Due to string interning, strings that have identical contents are the same Lox object, and equality naturally indicates identity.

Note also that slicing notation is desugared into a Slice object construction, and therefore the following lines are equivalent:

```
hashmap[:] = true;
hashmap.set(Slice(nil, nil, 1), true);
```

This is not idiomatic use and should be avoided. Generally, avoid use of mutable data structures as hashmap keys.

A future addition will enable user-defined construction of immutable data classes for use in hashmaps.