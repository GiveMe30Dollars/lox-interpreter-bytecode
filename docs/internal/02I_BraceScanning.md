# 02I: Brace Scanning and Parsing for String Interpolation

## Scanning

After the addition of string interpolation (refer to document 04E), scanning is no longer stateless.

`}` could refer to either the end of a block quote, or the end of a string interpolation. Either case has different behaviour: for the block quote, make a `TOKEN_RIGHT_BRACE`; for the string interpolation, move beyond the `}` and scan the rest of the string.

To this end, the scanner is changed from clox in the following ways:
- `uint32_t braces`: This field conceptually represents a 256-length `bool` array, used as a stack. We manipulate it via bitmasking. initialized to 0.
- `uint8_t braceIdx`: The index past the topmost element of the `braces` "stack".

There are changes in scanning behaviour for strings and braces:  
- `"`: found beginning of string literal. consume and call scanString.
- `scanString()`:
  - advance until either of these are encountered:
    - `"`: closing mark. consume and make `TOKEN_STRING` *without* quotation marks.
    - `${`: string is interpolated. 
      - consume delimiters and make `TOKEN_INTERPOLATION`.
      - set current brace slot to `1`, increment `braceIdx`.
- `{`: actually just `TOKEN_LEFT_BRACE`.
  - current brace slot is implicitly `0`. increment `braceIdx`.
- `}`: depends.
  - decrement `braceIdx`, get current brace slot.
    - case `0`: make `TOKEN_RIGHT_BRACE`, return.
    - case `1`: end of string interpolation.
      - reset current brace slot to 0.
      - consume right brace, call scanString.

These changes are found in lines 132-161, 254-277, and 306. The bitmasking operations described above are done using:

```
scanner.braces |= (1 << scanner.braceIdx++);                             // String interpolation

scanner.braceIdx++;                                                      // Left brace

bool isInterpolation = scanner.braces & (1 << (--scanner.braceIdx));     // Right brace decrement
if (isInterpolation){
    // ...
    scanner.braces ^= (1 << scanner.braceIdx);                           // Right brace reset
    // ...
}
```

Hence, as suggested in the [original solution](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter16_scanning.md),

<blockquote>
This:<br>
<code>
    "Tea will be ready in ${steep + cool} minutes."
</code>
Gets scanned like:<br>
<code>
    TOKEN_INTERPOLATION "Tea will be ready in"
    TOKEN_IDENTIFIER    "steep"
    TOKEN_PLUS          "+"
    TOKEN_IDENTIFIER    "cool"
    TOKEN_STRING        "minutes."
</code>
</blockquote>

## Parsing

With reference to [Wren's implemetation](https://github.com/wren-lang/wren/blob/77aeb12ab8cff432dcc0e0c511d0f30366650f15/src/vm/wren_compiler.c#L2436).

The pseudocode is as such:

```
// we came here from the Pratt Parser
// the TOKEN_INTERPOLATION is in parser.previous

get idxC, idxS for constants "concatenate" and "string" (native functions)
let argc = 0

emit OP_GET_GLOBAL idxC

do {
    call string() to parse the TOKEN_INTERPOLATION as a string
    emit OP_GET_GLOBAL idxS
    call expression(), evaluating and advancing all of the code segment
    emit OP_CALL 1
    assert argc will not overflow the 8-bit integer limit
    argc += 2
} while match(TOKEN_INTERPOLATION)

advance()
call string() for last TOKEN_STRING
argc += 1;

emit OP_CALL argc
```

This is found in compiler.c.

## Concatenate

`concatenate()` is a true variadic function; it has no arity checking, and can take from 0 up to 255 arguments. Calling it with no arguments returns an empty string.

This can be done because of how native functions are passed their arguments: an integer `argCount`, and a pointer into the stack for the first argument. Just like C-style `int argc` and `const char *argv[]`.

*Aside: Lox does not allow users to define variadic functions or overloaded functions. I'm not sure how I should implement such a thing, or whether I even could. Lox does not have pointers, and I'm not familiar with the syntax around this for other languages. Oh well.*

`concatenate()` walks the arguments twice: first to sum their lengths for a sufficient `char*` allocation, then a second to copy their contents using `memcpy`. The result is wrapped as an `ObjString*` then as a `Value` before returning.