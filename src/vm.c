#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

// global variable
VM vm;

// INITIALIZE/FREE VM
static void resetStack(){
    vm.stackTop = vm.stack;
}
void initVM(){
    resetStack();
    initTable(&vm.globals);
    initTable(&vm.strings);
    vm.objects = NULL;
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
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, (int)instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}


static InterpreterResult run(){

    // Preprocessor macros for reading bytes
    #define READ_BYTE() (*vm.ip++)
    #define READ_SHORT() (vm.ip += 2, (uint16_t)vm.ip[-2] << 8 | vm.ip[-1])
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
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
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
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
                uint8_t idx = READ_BYTE();
                push(vm.stack[idx]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t idx = READ_BYTE();
                vm.stack[idx] = peek(0);
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
                if (isFalsey(peek(0))) vm.ip += jump;
                break;
            }
            case OP_JUMP: {
                uint16_t jump = READ_SHORT();
                vm.ip += jump;
                break;
            }
            case OP_LOOP: {
                uint16_t jump = READ_SHORT();
                vm.ip -= jump;
                break;
            }
                
            case OP_RETURN:{
                // TODO: functions
                return INTERPRETER_OK;
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
    Chunk chunk;
    initChunk(&chunk);
    if(!compile(source, &chunk)){
        freeChunk(&chunk);
        return INTERPRETER_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;
    InterpreterResult result = run();

    freeChunk(&chunk);

    return INTERPRETER_OK;
}