#include <stdio.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"

// global variable
VM vm;

static void resetStack(){
    vm.stackTop = vm.stack;
}
void initVM(){
    resetStack();
}

void freeVM(){
    // currently empty
}

void push(Value value){
    *vm.stackTop = value;
    vm.stackTop++;
}
Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}



static InterpreterResult run(){
    // Preprocessor macros for reading bytes
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define BINARY_OP(op)\
        do{ \
            double b = pop(); \
            double a = pop(); \
            push(a op b); \
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
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(+); break;
            case OP_MULTIPLY: BINARY_OP(+); break;
            case OP_DIVIDE:   BINARY_OP(+); break;

            case OP_NEGATE:   push(-pop()); break;

            case OP_RETURN:{
                printValue(pop());
                printf("\n");
                return INTERPRETER_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
}
InterpreterResult interpret(const char* source){
    compile(source);
    return INTERPRETER_OK;
}
/*
InterpreterResult interpret(Chunk* chunk){
    // wrapper to main function run()
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}
*/