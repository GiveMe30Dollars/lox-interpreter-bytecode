# 12E: Array Slicing

Experimental branch `containers` now supports array slicing, borrowing from Python syntax. Negative indexing is also supported with the same semantics as Python.

```c++
var arr = [0,1,2,3,4,5,6,7,8,9];

print arr[:5];          // [0,1,2,3,4]
print arr[5:];          // [5,6,7,8,9]
print arr[3:-3];        // [3,4,5,6]

print arr[::2];         // [0,2,4,6,8]
print arr[::-2];        // [9,7,5,3,1]
print arr[3:-1:-1];     // equivalent to print arr[3:9:-1], == []
```

Unlike Python, for setting the value of an array slice, the target must be an array of length equivalent to the length of the slice applied to the array. Failure to do so is a runtime exception.

Currently, compound assignment for array slicing is not supported. This is because `Array` currently does not implement the functions to overload the arithmatic operators.

Note that slice notation is syntactic sugar for invoking the `get` or `set` method with a slice object. User-defined classes may also use slice notation if the defined `get` method accepts slice objects as argument.

```c++
arr = [0,1,2,3,4,5,6,7,8,9]
target = ["zero", "two", "four", "six", "eight"]
// These two lines are equivalent
arr[::2] = target;
arr.set(Slice(nil, nil, 2), target);
```

A slice object accepts three arguments corresponding to `start : stop : step`, which must be whole numbers or `nil`. Additionally, the value of `step` must be non-zero, with `nil` corresponding to a default value of 1.

String indexing and slicing is also added, though only get operations are supported, as Lox strings are immutable. You may pass a string to the Array constructor to obtain an array of its substituent characters. As Sulfox does not have characters as a data type, this corresponds to strings of length 1.

Currently, there is no official way to convert an array of characters to a string. You may use `Array.reduce(fn)` to convert an array to a string via a string concatenation lambda.

```c++
var str = "Hello World!"
print str[::-1];           // !dlroW olleH
try {
    str[4] = ",";          // This fails. Runtime exception thrown
} catch(err) {
    print err;
}

var arr = Array(str);
arr[4] = ","
var newStr = arr.reduce(fun(m, n){"${m}${n}"});
print newStr               // Hell, World!
```

Refer to Document [17I](../internal/17I_ArraySlicing.md) for implementation details.