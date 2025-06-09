# 00E: Overview
Lox is a dynamically-typed language with automatic memory management and class-based object-oriented programming.  
For more information, visit: https://craftinginterpreters.com/the-lox-language.html

This section documents new additions to the implemnetation that may be distinct from the original Lox specification.

## Common Flags

The following flags in `common.h` changes the behaviour of the VM:

- **`DEBUG_TRACE_EXECUTION`**: (Verbose) Enables line-by-line printing of stack state during VM runtime.
- **`DEBUG_PRINT_CODE`**: (Verbose) Enables printing of chunk disassembly onto stdout upon successful compilation.
- **`IS_FALSEY_EXTENDED`**: Enables `Number(0)`, `String("")` and inaccessible type `<empty>` to be evaluated as Boolean `false` for boolean operations.