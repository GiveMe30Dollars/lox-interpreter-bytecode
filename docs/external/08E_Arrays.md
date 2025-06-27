# 08E: Arrays

The experimental branch `containers` now supports arrays as first-class data types.

Arrays can be initialized using array literal syntax `[...]` or using the `Array` constructor.

```c++
var arr = [0,1,2,3,4];
var arrTwo = Array(5);
print arrTwo;             // [nil, nil, nil, nil, nil]
```

Array elements may be of any mixed types.

```c++
var mixed = [nil, true, false, 3.1415, "Hello World!", ["another", "array"]];
```

Array elements can be get and set using subscript notation. This also supports negative indexes, but will fail if the index exceeds the bounds of the array. The index must be a whole number.

```c++
var arr = [0,1,2,3,4];
arr[-1] = 999;
print arr[4];            // 999

/*
These all result in runtime errors.
print arr[5];
print arr[-100];
print arr[3.1415];
print arr["Not an index!"];
*/
```

Subscript notation desugars to the `get` and `set` native methods, which may also be directly used:

```c++
var arr = [0,1,2,3,4];
print arr.get(0);        // Gets the value of arr[0]
arr.set(3, "333");       // Sets arr[3] to "333"
```

Any class that implements `get` and `set` methods may use subscript notation.

```c++
class Range {
    init(start, stop, step){
        this.start = start;
        this.stop = stop;
        this.step = step;
    }
    get(idx){
        var num = this.step * idx;
        var maxRange = stop - start;
        return num > maxRange ? maxRange : num; 
    }
}
print Range(0,20,2)[4];  // 8
```

Refer to Document [11I](../internal/11I_Arrays.md) for implementation details.