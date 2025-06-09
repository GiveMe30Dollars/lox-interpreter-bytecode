# 02E: Compound Assignment

Support has been added for the following compound assignments:

- `a += b`
- `a -= b`
- `a *= b`
- `a /= b`

These are syntactic sugar and are equivalent to their unwrapped forms.  
eg. `a += b` is equivalent to `a = a + b`.

Compound assignments may only be used on previously-declared variable `a`.  
eg. `var a += 5;` will not compile.