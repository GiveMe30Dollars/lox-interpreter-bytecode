#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

// global variable
VM vm;

// native function parsing
static void defineNative(const char* name, NativeFn function){
    // push onto stack to ensure they survive if GC triggered by reallocation of hash table
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
    pop();
    pop();
}

// native functions
static Value clockNative(int argCount, Value* args){
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}


// INITIALIZE/FREE VM
static void resetStack(){
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}
void initVM(){
    resetStack();
    initTable(&vm.globals);
    initTable(&vm.strings);
    vm.objects = NULL;

    defineNative("clock", clockNative);
}
void freeVM(){
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value){
    *vm.stackTop = value;
    vm.stackTop++;
}
Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}
static Value peek(int distance){
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value){
    #ifdef IS_FALSEY_EXTENDED
    // returns true for: false, nil, <empty>, 0, ""
    // (an enduser should not have access to <empty>)
    switch (value.type){
        case VAL_NIL:    return true;
        case VAL_BOOL:   return !AS_BOOL(value);
        case VAL_EMPTY:  return true;
        case VAL_NUMBER: return AS_NUMBER(value) == 0;
        case VAL_OBJ:    return AS_STRING(value)->length == 0;
        default:         return true;    // Unreachable.
    }
    #else
    // returns true for false and nil
    return (IS_NIL(value) || (IS_BOOL(value) && AS_BOOL(value) == false));
    #endif
}

static void concatenate(){
    // concatenates two strings on the stack
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static void runtimeError(const char* format, ...){
    // printf-style message to stderr
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // Stack trace
    for (int i = vm.frameCount - 1; i >= 0; i--){
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;

        fprintf(stderr, "[line %d] in ", getLine(&function->chunk, instruction));
        if (function->name == NULL){
            fprintf(stderr, "<script>\n");
        } else {
            fprintf(stderr, "%s\n", function->name->chars);
        }
    }

    resetStack();
}


static bool call(ObjFunction* function, int argCount){
    // attempts to write new call frame to VM
    
    // fail if arity not the same
    if (function->arity != argCount){
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }
    // stack overflow fail
    if (vm.frameCount == FRAMES_MAX){
        runtimeError("Stack overflow.");
        return false;
    }
    // success; create new call frame
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}
static bool callValue(Value callee, int argCount){
    // returns false if call failed
    if (IS_OBJ(callee)){
        switch(OBJ_TYPE(callee)){
            case OBJ_FUNCTION: 
                return call(AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= (argCount + 1);
                push(result);
                return true;
            }
            default: ;    // Fallthrough to failure case
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}


static InterpreterResult run(){

    // Get current call frame (of <script>)
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    // Store instruction pointer as native CPU register (TODO)
    // register uint8_t* ip

    // Preprocessor macros for reading bytes
    #define READ_BYTE() (*frame->ip++)
    #define READ_SHORT() (frame->ip += 2, (uint16_t)frame->ip[-2] << 8 | frame->ip[-1])
    #define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
    #define READ_STRING() (AS_STRING(READ_CONSTANT()))
    
    #define BINARY_OP(valueType, op)\
        do{ \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))){ \
                runtimeError("Operands must be numbers."); \
                return INTERPRETER_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(valueType(a op b)); \
        } while (false)


    // ip is the incrementer, and is changed internally
    for (;;){
        
        #ifdef DEBUG_TRACE_EXECUTION
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++){
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->function->chunk, 
            (int)(frame->ip - frame->function->chunk.code));
        #endif

        // excecute instruction
        uint8_t instruction;
        switch (instruction = READ_BYTE()){
            case OP_CONSTANT:{
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_NIL:   push(NIL_VAL()); break;
            case OP_TRUE:  push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP:   pop(); break;
            case OP_POPN: {
                vm.stackTop -= READ_BYTE();
                break;
            }

            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                Value name = READ_CONSTANT();
                Value value;
                if (!tableGet(&vm.globals, name, &value)){
                    runtimeError("Undefined variable '%s'", AS_CSTRING(name));
                    return INTERPRETER_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                if (tableSet(&vm.globals, name, peek(0))){
                    // isNewKey returned true. cannot set undeclared global variable.
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", AS_CSTRING(name));
                    return INTERPRETER_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;

            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))){
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;

            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))){
                    runtimeError("Operand must be a number.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                vm.stackTop[-1].as.number *= -1;
                break;
            case OP_UNARY_PLUS:
                if (!IS_NUMBER(peek(0))){
                    runtimeError("Operand must be a number.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // Does nothing.
                break;
            
            case OP_PRINT:
                printValue(pop());
                printf("\n");
                break;

            case OP_JUMP_IF_FALSE: {
                uint16_t jump = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += jump;
                break;
            }
            case OP_JUMP: {
                uint16_t jump = READ_SHORT();
                frame->ip += jump;
                break;
            }
            case OP_LOOP: {
                uint16_t jump = READ_SHORT();
                frame->ip -= jump;
                break;
            }
            
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // new call frame on stack
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_RETURN:{
                Value result = pop();
                vm.frameCount--;
                if (vm.frameCount == 0){
                    pop();
                    return INTERPRETER_OK;
                }
                // discard call frame, restore universal variables (stack top)
                // then push result
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}
InterpreterResult interpret(const char* source){
    ObjFunction* topLevelCode = compile(source);
    if (topLevelCode == NULL)
        return INTERPRETER_COMPILE_ERROR;

    // push top-level call frame onto stack
    call(topLevelCode, 0);

    return run();
}