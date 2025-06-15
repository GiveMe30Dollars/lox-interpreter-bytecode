#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "hashtable.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    // Can be ObjFunction or ObjClosure
    Obj* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    // Runtime fields
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    HashTable globals;
    HashTable strings;
    ObjUpvalue* openUpvalues;
    Obj* objects;
    uint32_t hash;

    // Garbage collector fields (we manage this ourselves)
    size_t bytesAllocated;
    size_t nextGC;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
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

InterpreterResult interpret(const char* source, bool evalExpr);

#endif