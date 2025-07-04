# 00I: Overview

The Lox interpreter consists of three main parts with state behaviour. State variables are typically not exposed or available to external files, and are contained within their implemetation files. Instances are initialized and fully owned by the subsequent part; the VM owns the compiler, the compiler owns the scanner.

Intermediate structures are used to pass information between the main parts, and are typically fully transparent in their structure.

## Scanning

Scanning takes in a C-string `\0`-terminated input and scans them for tokens. It is single-pass and acts as an iterator through the source string.

```c
// scanner.h
void initScanner(const char* source);
Token scanToken();

// scanner.c
typedef struct {
    const char* start;
    const char* curr;
    int line;
} Scanner;
```

- **`initScanner()`**: Initializes the scanner with the given C-string as Lox code.  
- **`scanToken()`**: Returns the next token in scanning.

# Tokens

Tokens are substrings of the code (lexemes) that are categorized by their TokenType.

```c
// scanner.h
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;
```
Note: `start` points directly into the source string. No strings are copied during scanning, and the source string survives until at least compilation completes.

# Compilation

Compilation takes in Tokens from the Scanner and parses them using Pratt Parsing. The rules for Pratt Parsing are encoded within `parseRules` ni `vm.c`, which returns function pointers for the prefix and infix rules corresponding to a TokenType, with their precedence ranked by ascending binding power in the enum Precedence.

```c
// compiler.h
bool compile(const char* source, Chunk* chunk);
```

There exists several private transient states that are destroyed upon completion of compilation:

### Parser

The Parser tracks the previous and current tokens parsed, as well as parsing-related behaviour flags.

```c
typedef struct {
    Token previous;
    Token current;
    bool hasError;
    bool panic;
} Parser;
```

### Compiler

The Compiler acts as a mirror to the Virtual Machine's stack for resolving local scope of variables.

```c
typedef struct {
    Token name;
    int depth;
} Local;
typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;
```

### Constant De-duplication

# Compiler-VM Coupling

### Bytecode Chunk

The Chunk contains most of the compilation information required to run the Lox VM.

```c
// chunk.h
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    LineInfo lineInfo;
    ValueArray constants;
} Chunk;
```

- **`count`, `capacity`**: Used to dynamically resize the Chunk during compilation.  
- **`code`**: The bulk of the Chunk; a sequence of simple bytecode instructions, starting with an `Opcode` and optionally followed by byte operands.  
- **`lineInfo`**: Stores the line information of each bytecode instruction using run-length encoding. Used for runtime errors and disassembly.  
- **`constants`**: A dynamic array storing the constant values used in the bytecode chunk. The opcode `OP_CONSTANT` and derivatives refer to contants by index in this array.

### Lox Functions

Chunks are encapsulated in Lox functions, which are created at compile-time and passed to the VM:

```c
// object.h
typedef struct{
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;
```

- **`obj`**: Every Lox object has one of these in their headers. This allows for quick upcasting and downcasting between its specialized pointer and the general `Obj*` pointer, which is what is stored in a Lox `Value`.
- **`arity`**: The number of arguments this Lox object expects. Top-level code expects 0 arguments.
- **`name`**: The variable name this Lox object is bound to.

### String Intern Table

```c
// hashtable.h
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} HashTable;
//...
ObjString* tableFindString(HashTable* table, const char* string, int length, uint32_t hash);

// in objects.h
ObjString* copyString(const char* start, int length);
ObjString* takeString(char* start, int length);
// in objects.c
static ObjString* allocateString(char* chars, int length, uint32_t hash){...}

// in vm.h
typedef struct{
    //...
    HashTable strings;
    //...
}
```
To ensure de-duplication, such that strings with the same characters have identical pointers to the same `ObjString`, the VM maintains the hash set `vm.strings`.
- **`tableFindString()`**: returns the unique pointer to the ObjString string corresponding to the C-string. If not found, returns `NULL`.
- **`copyString()`, `takeString()`**: calculates the hash of the C-string given, and looks up `vm.strings` via `tableFindString()`. If the string is in the hash set, return that pointer. Otherwise, call `allocateString()`.  
  `copyString()` creates a deep copy of the C-string. `takeString()` assumes that the resultant ObjString has sole ownership of the given C-string and does not copy it.
- **`allocateString()`**: allocates and creates the ObjString on C heap space. Then, sets `vm.strings[str]` for this instance.

This happens during compile-time for statically-defined strings and identifiers to optimize string comparisons. This is also done during runtime during concatenation of strings.  
It is possible, though not immediatly helpful, to decouple the string intern table such that the Compiler and Virtual Machine maintains one each.

# Virtual Machine

The Lox Virtual Machine executes stack-based bytecode in Chunk.

```c
typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    HashTable globals;
    HashTable strings;
    Obj* objects;
} VM;

extern VM vm;

InterpreterResult interpret(const char* source);
```

- **`stack`**: `stackTop` and all `frames[i].slots` point into this stack.
- **`objects`**: Linked list of all Lox objects allocated during compile-time and runtime.
- **`strings`**: Aformentioned string intern table.
- **`globals`**: Stores the key-pair entries of global variables.

A CallFrame is created every time a function is called. When a function returns, the call frame is discarded. If the call frame of the top-level code has finished execution and is discarded, exit the VM.