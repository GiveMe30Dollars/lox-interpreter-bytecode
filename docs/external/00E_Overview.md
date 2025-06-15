# 00E: Overview
Lox is a dynamically-typed language with automatic memory management and class-based object-oriented programming.  
For more information, visit: https://craftinginterpreters.com/the-lox-language.html

This section documents new additions to the implemnetation that may be distinct from the original Lox specification.

## Common Flags

The following preprocessor flags in `common.h` changes the behaviour of the VM:

- **`DEBUG_TRACE_EXECUTION`**: (Verbose) Logs line-by-line execution and stack state onto `stdout` during VM runtime.  
  **Off** by default.

- **`DEBUG_PRINT_CODE`**: (Verbose) Prints of chunk disassembly onto `stdout` upon successful compilation.  
  **Off** by default.

- **`DEBUG_STRESS_GC`**: Calls garbage collection every time a new allocation is made.  
  **Off** by default.

- **`DEBUG_LOG_GC`**: (Verbose) Logs any allocation, deallocation, marking and traversal of objects onto `stdout`.  
  **Off** by default.

- **`OBJ_HEADER_COMPRESSION`**: Compresses the `Obj` header shared by all heap-allocated object types from 16 bytes to 8 bytes.  
  *Assumes 64x host architecture. Assumes at most 48 little-endian bits used for 64-bit pointers.*  
  **On** by default.

- **`IS_FALSEY_EXTENDED`**: Enables `Number(0)`, `String("")` and inaccessible type `<empty>` to be evaluated as Boolean `false` for boolean operations.  
  **Off** by default.