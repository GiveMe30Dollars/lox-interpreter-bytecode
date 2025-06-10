# 01I: Opcodes

The Lox VM is a primarily stack-based virtual machine of size 256.  
For the sake of brevity, stack refers to `vm.stack`.  
Global variables are stored in the `vm.globals` hashtable.
(TODO: extend stack to 24-bit limit?)

The currently implemented operation codes (opcodes) are as follows:

- **`OP_CONSTANT`** `idx` : Pushes the value of `chunk->constants[idx]` onto the stack.
- **`OP_NIL`**: Pushes Lox `nil` onto the stack.
- **`OP_TRUE`**: Pushes Lox `true` onto the stack.
- **`OP_FALSE`**: Pushes Lox `false` onto the stack.
- **`OP_POP`**: Pops the topmost element off the stack.
- **`OP_POPN`** `num`: Pops the topmost `num` elements off the stack.

- **`OP_DEFINE_GLOBAL`** `idx`: Defines a Lox global variable of name `chunk->constants[idx]` with value of the stack top.
- **`OP_GET_GLOBAL`** `idx`: Gets the value of a Lox global variable of name `chunk->constants[idx]`.
- **`OP_SET_GLOBAL`** `idx`: Sets the value of a Lox global variable of name `chunk->constants[idx]` to the value of the stack top.
- **`OP_GET_LOCAL`** `idx`: Gets the value of a Lox local variable from `vm.stack[idx]`.
- **`OP_SET_LOCAL`** `idx`:  Sets the value of a Lox local variable from `vm.stack[idx]` to the value of the stack top.

- **`OP_EQUAL`**: Pops the topmost two elements from the stack and evaluates `a == b`. Pushes the result onto the stack.
- **`OP_GREATER`**: Pops the topmost two elements from the stack and evaluates `a > b`. Pushes the result onto the stack.
- **`OP_LESS`**: Pops the topmost two elements from the stack and evaluates `a < b`. Pushes the result onto the stack.

- **`OP_ADD`**: Pops the topmost two elements from the stack and evaluates `a + b`. Pushes the result onto the stack.
- **`OP_SUBTRACT`**: Pops the topmost two elements from the stack and evaluates `a - b`. Pushes the result onto the stack.
- **`OP_MULTIPLY`**: Pops the topmost two elements from the stack and evaluates `a * b`. Pushes the result onto the stack.
- **`OP_DIVIDE`**: Pops the topmost two elements from the stack and evaluates `a / b`. Pushes the result onto the stack.

- **`OP_NOT`**: Pops the topmost element and evaluates `!boolean`. Pushes the result onto the stack.
- **`OP_NEGATE`**: Pops the topmost element and evaluates `- number`. Pushes the result onto the stack.
- **`OP_UNARY_PLUS`**: Asserts the topmost element is of type `Number`. Does nothing.

- **`OP_PRINT`**: Pops the topmost element and prints its value.
- **`OP_JUMP_IF_FALSE`** `byteX2`: Moves the instruction pointer (`vm.ip`) forwards by `byteX2` bytes if the top of the stack evaluates to `false`.
- **`OP_JUMP`** `byteX2`: Moves the instruction pointer (`vm.ip`) forwards by `byteX2` bytes.
- **`OP_LOOP`** `byteX2`: Moves the instruction pointer (`vm.ip`) backwards by `byteX2` bytes.

- **`OP_CALL`** `argc`: The stack is arranged such that there exists a `function` object, followed by `argc` arguments, at the top of the stack. Call `function` by pushing a new CallFrame onto the call stack for these values.
- **`OP_RETURN`**: Pops the call stack and returns the topmost element in the popped frame. Exit the VM and return `INTERPRETER_OK` if the popped frame is executed top-level code.