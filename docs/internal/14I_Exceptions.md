# 14I: Exceptions

Funnily enough, I didn't have to change much to get this one working. The tools are here, i just needed to compose them into something that fits the semantics of try-throw-catch control flow.

I wanted this to work not only on the Lox end, but also on the C end; my native functions can signal failure, but what should I do afterwards with that information?

The naive approach would be to immediately terminate the program. That's fine, but it pushes a lot of the responsibility of type-checking onto the Lox end-user. Something like `"str" + 3` would be immediately fatal. As part of the STL also involves exposing IO functionality, I think this would be a chore to work around for a Lox end-user. They'd have to defensively program a lot of things before passing it to the Lox core operators. That's not great.

So, if there *is* something that is bad but not irrevocable, we have the VM throw a runtime *exception* instead. This would unwind the call stack up until a try clause is found, and from there the end-user can do error recovery or logging in the catch clause.

Okay, so what does a try clause do? It has a separate lexical scope that is discarded when something inside throws an exception.

Wait a minute.

## Try Clauses Are (Tagged) Anonymous Functions

Yep. 

This does necessitate adding an additional boolean field `bool fromTry` in `ObjFunction` for whether this function/closure is created from a try clause. Beats messing with the call frames themselves or adding a new data type, though.

```
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    bool fromTry;              // <----
    Chunk chunk;
    ObjString* name;
} ObjFunction;
```

We compile a try clause as a function with this field set. For every other situation where we use functions, this field is false by default. Once we load the try clause-function onto the stack, we call it using a new opcode `OP_TRY_CALL`. 

`OP_TRY_CALL` behaves identically to `OP_CALL` sans type-checking and argument count (a try clause has arity 0), but its an indication to us language hackers that this opcode is different from `OP_CALL` in *how* it's emitted. `OP_TRY_CALL` is **always** followed by four bytes: `OP_POP` and `OP_JUMP` `byteX2`. If we return here from executing the entirity of the try clause, this let's us jump past the bytecode of the catch clause. If we are *thrown* here, some additional logic in the VM will skip past these 4 bytes instead, leading the instruction pointer to the catch clause.

Then, for the catch clause, we can cheap out and compile it as a block, with a synthetic local variable depending on the identifier in the parentheses.

## Runtime Exceptions (Throwing)

Compiling the `throw` statement is easy. It's near identical to a return statement, with `OP_THROW` as the operand instead of `OP_RETURN`.Now to figure out what that should do during runtime. 

Since we're going to call this from the VM as well, let's separate this off into its own function.

```
static bool throwValue(Value* payload){
    // What goes here?
}
```

We start from the call frame where we threw the exception and walk outwards. If the callframe's function does not have `fromTry` set, it's not a try clause and we continue walking. If we unwind all the way out and don't encounter any try clauses, we promote the exception to a runtime error. This also conveniently prints out the stack trace, starting from the function where the exception is thrown.

If we *do* encounter a try clause, we discard all call frames above it *and* the try clause itself, setting the stack top to the 0th stack slot of the try clause. Then, we push the payload onto the stack.

We need a way to indicate whether the exception throw leads to a valid state where the interpreter can continue executing. This is what the return value is for: `true` if the throw is caught and to continue execution, and `false` otherwise. The choice to have true indicate a successfully *caught* exception is a bit counterintuitive, but lines up with the semantics of `call` and `invoke` helper functions. That means that we can return a tail call to `throwValue` if these methods throw a runtime exception.

For the sake of easier plumbing through the VM, I also added another helper function `runtimeException` that mimics the semantics of `runtimeError` by constructing and throwing an `ObjString`.

```
static bool runtimeException(const char* format, ...){
    // create a printf-style message to an ObjString, pushed onto the stack
    // then, throw it.
    va_list args;

    va_start(args, format);
    int bufferSize = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char* buffer = ALLOCATE(char, bufferSize + 1);
    va_start(args, format);
    vsnprintf(buffer, bufferSize + 1, format, args);
    buffer[bufferSize] = '\0';
    va_end(args);

    Value payload = OBJ_VAL(takeString(buffer, bufferSize));
    // push onto stack to prevent garbage collector headaches
    push(payload);
    return throwValue(vm.stackTop - 1);
}
```

As well as some preprocessor macros to save and load the `ip` from the topmost callframe to a local register pointer as described in [Challenge 24-1](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter24_calls/1.md):

```
// in run():
    #define SAVE_IP()       (frame->ip = ip)
    #define LOAD_IP() \
        do { \
            frame = &vm.frames[vm.frameCount - 1]; \
            ip = frame->ip; \
        } while (false)
    #define THROW(runtimeCall) \
        do { \
            SAVE_IP(); \
            if (runtimeCall == false) return INTERPRETER_RUNTIME_ERROR; \
            else LOAD_IP(); \
        } while (false)
```

Then, whenever we want to throw a runtime exception in run(), we can use them.

```
// in run():
            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                HashTable* table = isSTL ? &vm.stl : &vm.globals;
                if (tableSet(table, name, peek(0))){
                    tableDelete(table, name);
                    THROW(runtimeException("Undefined variable '%s'.", AS_CSTRING(name)));          // <----
                    break;                                                                          // <----
                }
                break;
            }
```

That's pretty clean if I do say so myself.