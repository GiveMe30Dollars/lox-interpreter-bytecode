# 01I: Opcodes

The Lox VM is a primarily stack-based virtual machine of size 256.  
For the sake of brevity, stack refers to `vm.stack`.  
Global variables are stored in the `vm.globals` hashtable.

The following arguments are shorthand:
- `idx`: A single-byte operand representing a generic number index to a specified container.
- `cidx`: A special form of `idx` that points to the value constant stored in `chunk->constants[cidx]`. Single-byte operand.
  - On the `long-opcodes` branch, all opcodes that take in `cidx` also has a `LONG` variant, which uses a three-byte operand instead.
- `byteX2`: A two-byte operand. Typically used in jump operations.

The currently implemented operation codes (opcodes) are as follows:

- **`OP_CONSTANT`** `cidx` : Pushes the value of `chunk->constants[cidx]` onto the stack.
- **`OP_NIL`**: Pushes Lox `nil` onto the stack.
- **`OP_TRUE`**: Pushes Lox `true` onto the stack.
- **`OP_FALSE`**: Pushes Lox `false` onto the stack.
- **`OP_DUPLICATE`**: Duplicates the topmost element and pushes it onto the stack.
- **`OP_POP`**: Pops the topmost element off the stack.
- **`OP_POPN`** `num`: Pops the topmost `num` elements off the stack.

- **`OP_DEFINE_GLOBAL`** `cidx`: Defines a Lox global variable of name `chunk->constants[cidx]` with value of the stack top.
- **`OP_GET_GLOBAL`** `cidx`: Gets the value of a Lox global variable of name `chunk->constants[cidx]`.
- **`OP_SET_GLOBAL`** `cidx`: Sets the value of a Lox global variable of name `chunk->constants[cidx]` to the value of the stack top.
- **`OP_GET_LOCAL`** `idx`: Gets the value of a Lox local variable from `frame->slots[idx]`.
- **`OP_SET_LOCAL`** `idx`:  Sets the value of a Lox local variable at `frame->slots[idx]` to the value of the stack top.
- **`OP_GET_UPVALUE`** `idx`: Gets the value of a Lox upvalue from `*frame->closure->upvalues[idx]->location`.
- **`OP_SET_UPVALUE`** `idx`:  Sets the value of a Lox upvalue at `*frame->closure->upvalues[idx]->location` to the value of the stack top.

- **`OP_EQUAL`**: Pops the topmost two elements from the stack and evaluates `a == b`. Pushes the result onto the stack.
- **`OP_GREATER`**: Pops the topmost two elements from the stack and evaluates `a > b`. Pushes the result onto the stack.
- **`OP_LESS`**: Pops the topmost two elements from the stack and evaluates `a < b`. Pushes the result onto the stack.

- **`OP_ADD`**: Pops the topmost two elements from the stack and evaluates `a + b`. Pushes the result onto the stack.
- **`OP_SUBTRACT`**: Pops the topmost two elements from the stack and evaluates `a - b`. Pushes the result onto the stack.
- **`OP_MULTIPLY`**: Pops the topmost two elements from the stack and evaluates `a * b`. Pushes the result onto the stack.
- **`OP_DIVIDE`**: Pops the topmost two elements from the stack and evaluates `a / b`. Pushes the result onto the stack.

- **`OP_NOT`**: Pops the topmost element and evaluates `!boolean`. Pushes the result onto the stack.
- **`OP_NEGATE`**: Pops the topmost element and evaluates `- number`. Pushes the result onto the stack.

- **`OP_PRINT`**: Pops the topmost element and prints its value.
- **`OP_JUMP_IF_FALSE`** `byteX2`: Moves the instruction pointer (`vm.ip`) forwards by `byteX2` bytes if the top of the stack evaluates to `false`.
- **`OP_JUMP`** `byteX2`: Moves the instruction pointer (`vm.ip`) forwards by `byteX2` bytes.
- **`OP_LOOP`** `byteX2`: Moves the instruction pointer (`vm.ip`) backwards by `byteX2` bytes.

- **`OP_CALL`** `argc`: The stack is arranged such that there exists a `callable` object, followed by `argc` arguments, at the top of the stack.
  - For Lox functions, call `callable` by pushing a new CallFrame onto the call stack for these values.
  - For Lox native functions, call `callable` inplace, without pushing a new CallFrame. The result is written to the stack.
  - For Lox classes, create a new Lox instance inplace of `callable`, then call the initializer of the class.
  - For Lox bound methods, set reserved slot 0 to the bound instance, then call the contained function.

- **`OP_CLOSURE`** `cidx` `upvalueIsLocal` `upvalueSlot` `...` : Creates an `ObjClosure*` at runtime from the function at `chunk->constants[cidx]`. Then, for each upvalue defined in that function, take a pair of byte operands to capture the upvalues: 
  - `upvalueIsLocal` determines whether the VM should search for a local variable: 
    - `1` for walking through the current open upvalues, and creating a new one if not currently present, 
    - `0` for inheriting an upvalue from the current enclosing function.
  - `upvalueSlot` depends on whether this upvalue is for a local variable or an inherited upvalue: 
    - for local variables, it's its position on the stack relative to the current enclosing `CallFrame`, 
    - for inherited upvalues, the index in `frame->closure->upvalues`.
-  **`OP_CLOSE_UPVALUE`**: closes an upvalue by popping it from the stack and transferring it into the `ObjUpvalue*` struct on the heap.

- **`OP_RETURN`**: Pops the call stack and returns the topmost element in the popped frame. Exit the VM and return `INTERPRETER_OK` if the popped frame is top-level code.

- **`OP_CLASS`** `cidx`: Declares a new Lox class of name `chunk->constants[cidx]`
- **`OP_GET_PROPERTY`** `cidx`: Gets the property of identifier `chunk->constants[cidx]` for the instance at the top of the stack.
  - Fields shadow methods.
  - If a method is fetched, bind it to this instance using an `ObjBoundMethod`.
- **`OP_SET_PROPERTY`** `cidx` : Sets the field of identifier `chunk->constants[cidx]` for the instance in the topmost 2nd slot to the value at the top of the stack. Pops the instance from the stack.
- **`OP_METHOD`**: Defines a function object at the top of the stack to be a method of the class underneath. Pops the function object.
- **`OP_INVOKE`** `cidx` `argc`: Optimized combination for `OP_GET_PROPERTY` and `OP_CALL`.
  - The stack is arranged such that an instance object, followed by `argc` call arguments are on top of the stack.
  - Get the method `chunk->constants[cidx]` through this instance, and invoke it without creating an `ObjBoundMethod`.
  - Fields shadow methods. If a field of this name is found, decompose to unoptimized call path.

- **`OP_INHERIT`**: Causes a class to inherit another.
  - The stack is arranged such that `superclass` and `subclass` are on top of the stack.
  - Let subclass inherit superclass, and pop subclass fromm stack. Superclass remains as local variable `super` for method closures.
- **`OP_GET_SUPER`** `cidx`: Superclass method access.
  - The stack is arranged such that the current instance object and the `superclass` are on top of the stack.
  - Get the method `chunk->constants[cidx]` found in `superclass`, and bind it to this instance using an `ObjBoundMethod`.
  - Pop both the instance and superclass objects.
- **`OP_SUPER_INVOKE`** `cidx` `argc`: Optimized combination of `OP_GET_SUPER` and `OP_CALL`.
  - The stack is arranged such that the current instance object, `argc` call arguments and its superclass are on top of the stack.
  - Gets the method `chunk->constants[cidx]` from the superclass, pop the superclass, and invoke it without creating an `ObjBoundMethod`.