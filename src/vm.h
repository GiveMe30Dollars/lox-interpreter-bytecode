#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "hashtable.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    HashTable globals;
    HashTable strings;
    Obj* objects;
} VM;

extern VM vm;

void initVM();
void freeVM();
void push(Value value);
Value pop();

typedef enum {
    INTERPRETER_OK,
    INTERPRETER_COMPILE_ERROR,
    INTERPRETER_RUNTIME_ERROR
} InterpreterResult;

InterpreterResult interpret(const char* source);

#endif