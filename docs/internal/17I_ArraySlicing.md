# 17I: Array Slicing

This seems simple, right? It is, it's just that we have to wade through some semantics.

## Python Slices are Kinda Confusing, Actually

I thought that I was certain that I wanted to borrow Python's syntax for array slicing. For the way the compiler is currently set up, it was easier to parse Python-style `arr[start:stop:step]` (even if it is a bit of a pain; refer to next section) rather than Perl-style `arr[start..end]`. The latter would necessitate adding a new token `TOKEN_DOT_DOT` to disambugate from `TOKEN_DOT`, especially when you start using property getters in the expression for the slice:

```
var arr = /* ... */
var instance = SomeClass();

arr[instance.first..instance.second];     // I mean, anyone who wants to make this readable should add spacing:
arr[instance.first .. instance.second];   // but this parses identically to the previous line.
```

Question, though: should an array slice be end-inclusive?

The syntax for array slicing is a bit all over the place, especially as most languages that implement it are either not meant for scripting or use paradigms other than zero-based indexing *in addition* to negative indexes. I'll only be looking at Python and Perl.

The Python approach does have some nice properties, especially with regards to half-open intervals and how `arr == concatenate(arr[:pivot], arr[pivot:])`, as well as that `step` argument. It makes indexing especially easy for every nth element (`arr[::n]`) or getting a reversed list (`arr[::-1]`). The reason I lean towards this is due to familiarity as well: Python is one of my languages alongside Java and C++/C.

There's two main cons against Python-style slicing:
- There's quite a few cases to parse for, which is extra work done by the compiler. There is exactly one case for Perl-style slicing, two if you count base indexing. Also, Perl supports indexing using a function, equivalent to a `filter` operation in current-day Lox.
- Half-open intervals are *extremely unintuitive*, especially for beginners. [Here's a StackOverflow post all about that.](https://stackoverflow.com/questions/11364533/why-are-pythons-slice-and-range-upper-bound-exclusive) One commentor aptly claimed that it boiled down to "Elegant-ness vs. Obvious-ness", using the task of splitting an array as an example:
    ```python
    # Python
    arr   = [0,1,2,3,4]
    pivot = 2
    print(arr[ :pivot])       # [0,1]
    print(arr[pivot: ])       # [2,3,4]
    ```
    ```perl
    # Perl
    @arr   = [0,1,2,3,4];
    $pivot = 2;
    print "@arr[0..($pivot - 1)]\n";    # 0 1
    print "@arr[$pivot..-1]\n";         # 2 3 4
    ```
    *Aside: do try to ignore the Perl variable annotation. The comment in that StackOverflow post uses Ruby for a cleaner example. On the plus side, Perl treats print as a statement!*

While in the end I went with a Python-style approach, the Perl-style is also a valid approach that is used by other languages such as Ruby and Wren.

## Parsing Slice Notation

The parsing logic is written in the `subscript` compiler function, but the tricky part about array assignment is that its similar to a for statement, except that the second colon delimiter can be optional too:

| Slice Notation | Desugared Expression |
| -------------- | -------------------- |
| `arr[expr]`    | `arr.get(expr)`      |
| `arr[:]`       | `arr.get(Slice(nil, nil, nil))`  |
| `arr[:expr]`   | `arr.get(Slice(nil, expr, nil))` |
| `arr[expr:]`   | `arr.get(Slice(expr, nil, nil))` |
| `arr[e1:e2]`   | `arr.get(Slice(e1, e2, nil))`    |
| `arr[::]`      | `arr.get(Slice(nil, nil, nil))`  |
| `arr[e1:e2:]`  | `arr.get(Slice(e1, e2, nil))`    |
| `arr[e1:e2:s]` | `arr.get(Slice(e1, e2, s))`      |

The table above is non-exhaustive. I think you see my point.

I parse these clauses one at a time, assuming that a delimiter or the terminator `TOKEN_RIGHT_BRACKET` isn't checked. For the start clause specifically, if a colon is checked, it is a slice starting with nil (the beginning of the array) and we thus emit an `OP_NIL` instruction. The same is done for the end field, assuming that there *is* an end field where a colon is matched rather than just a single expression. The rest, I passed to a native static method invocation `Slice.@raw`, which does the following:

| Argument Count | Return Value |
| - | ---------- |
| 1 | `args[0]` |
| 2 | `Slice(args[0], args[1], nil)` |
| 3 | `Slice(args[0], args[1], args[2])` |

This handles the semantics of slice object creation while also letting single expressions remain unchanged as the `get` argument.

## Side Tangent: Array-Slicing Assignment

You can do some really cursed stuff with array-slicing assignment in Python. [Refer to this Reddit post](https://www.reddit.com/r/Python/comments/1fvyu8b/i_never_realized_how_complicated_slice/), but for the long-story-short, let's dissect this mindbender:

```python
>>> x = [1, 2, 3, 4, 5]
>>> x = x[1:-1] = x[1:-1]
>>> x
[2, 2, 3, 4, 4]
```  
*— u/TangibleLight, in the comment section of [the above post](https://www.reddit.com/r/Python/comments/1fvyu8b/i_never_realized_how_complicated_slice/).*
</blockquote>

A few things to note about Python:
- Assignment is a statement rather than an expression, so something like `i = (j = 0)` is a compile error. `:=` is the operator that implements the usual semantics of assignment expressions.
- Multiple assignment is inbuilt (for `=`), and applies *left to right*.
- Among other, more confusing things, setting to a range that is larger or smaller than the target inserts or deletes excess elements, respectively. This interacts in cursed ways with the step argument.

Both these run counter to most languages that inherit from C, where assignment via infix operator `=` is a right-associative expression.

So the above is equivalent to:

```python
>>> t = x[1:-1]  # t is [2, 3, 4]
>>> x = t        # x is [2, 3, 4]
>>> x[1:-1] = t  # expansion on middle element. [2] + [2, 3, 4] + [4]
```  
*— u/TangibleLight, in the same comment.*

... yeah.

I'm not sure if I want to allow some of these more esoteric uses of slices (setting to a range that is larger or smaller than the target). For now I have a check in the `set` operator to assert that the two are exactly the same length.

## Okay but Why `nil`?

Consider the following:

```c++
var arr = [0,1,2,3,4,5,6,7,8,9];
print arr[::2];
print arr[::-1];
```

Those two array slice `get` operations have different start and end points: `0` to `arr.length()` for the first, and `arr.length() - 1` to `-1` for the second. This is not something you can determine during the creation of the slice object, since a slice object is not inherently bound to an array and the values need to be calculated using information from an array. Therefore, `nil` correspond to these default values, which are calculated during the `get` or `set` method calls of the array.

## Wrapping Around?

What should this result in?

```
var arr = [0,1,2,3,4,5,6,7,8,9];
print arr[2:-2:-1];
```

It makes some intuitive sense to "wrap around" and get `[2,1,0,9]`, but we'll have to add conditional logic so that `arr[2:-2:1]` evaluates correctly to `[2,3,4,5,6,7]`.

There's a lot of edge-case hell that arises from allowing such operations, especially with assuming defaults; should `arr[2,-2]` correspond to `arr[2:-2:1]` or `arr[2:-2:-1]`? Either of these could be what the user expects, and we can't have both. Which, then? How about slices beginning from a negative index and ending in a positive one?

Keep in mind that while negative indexing is supported in Lox, *circular* indexing is not:

```
+-----+-----+-----+
| "a" | "b" | "c" |
+-----+-----+-----+
   0  ,  1  ,  2     <-- Positive Indexing
  -3  , -2  , -1     <-- Negative Indexing
```

Only indexes `-3 <= n < 3` for integer n are valid indexes. Anything beyond is out of range.

I'll be copying Python's semantics regarding this matter: `arr[2:-2:-1]` is equivalent to `arr[2:8:-1]`, which is empty array `[]`.