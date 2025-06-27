# 16I: Function Overloading

I did not want it to come to this.

At first, I thought that this would be wholly unecessary: since dynamic arrays are now first-class objects, we can use an array for any number of arguments. Native functions can also be secretly variadic, though this isn't exposed to the end-user (even if I use it liberally, what with `Array.@raw()` and all that).

What made me end up consider adding it was a confluence of the following:

- I want the Array initializer to be able to take in an iterator and construct an array, in the order that the iterator emits each element. This is so that a method like `.map(fn)` can be implemented via inheritance of the iterator protocol rather than any implementation-specific details of the container class itself.

- Iterators *cannot* be primitive objects. To make `.map(fn)` and `.filter(fn)` implementation-agnostic relative to the container class (other than the `iter` method and the specific iterator for that container type), the special iterators they use would inherit from a normal iterator for the container type but have additional logic for `hasNext` and `next`. For instance, `map`'s and `filter`'s iterator would look something like this:
    ```c++
    fun createMapIterator(container){
        var Iterator = type(container.iter());
        class MapIterator < BaseIterator {
            init(fn){
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

    fun createFilterIterator(container){
        var Iterator = type(container.iter());
        class FilterIterator < Iterator {
            init(fn){
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

- The array initializer has to take in one argument, and `Array(nil)` is counterintuitive and ugly as sin if `Array()` is *also* not available.

Funny thing is, I think I already know how to do it even before I had implemented it.

## Name-Mangling Functions

Anonymous functions and lambdas are not name-mangled, since they are compiled once and either remain on the stack as an expression or are assigned to a global variable. Either way, you cannot access them through a function name identifier anyways.

## Runtime Support and Reworking Arity Failures

The runtime now needs to do additional work for a function call. Keep in mind that after name-mangling, *this* is what the runtime sees:
```c++
fun fac@0x01(n){
    if (n <= 2) return n;
    return n * fac@0x01(n - 1);
}
fun fac@0x02(m,n){
    // I'm not sure why you'd do this, but ok.
    return fac(m + n);
}

fac(5, 1);
// Expect: 360
```

By the time we get to these function calls, `OP_CALL` and friends will be carry the byte operand necessary to resolve `fac` to the correct implementation of `fac@0x01` or `fac@0x02`.

Some calling code will have to be rewritten here.

Keep in mind that we still need the runtime arity check due to anonymous functions and lambdas, which is redundant work done. I could remove this, but I don't think it'd be significant enough of a runtime cost to justify optimizing.

## Shadows of Innsmouth

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



Should it be like this?

I can see arguments for and against this. Done purposefully, it could be rather powerful. It could also be the gateway to many frustrating logical bugs.