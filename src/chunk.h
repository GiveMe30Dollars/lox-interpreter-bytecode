#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// Contains all declarations regarding the Chunk class
// as well as the enum OpCode for bytecode

typedef enum {
    OP_CONSTANT,
        OP_CONSTANT_LONG,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_DUPLICATE,
    OP_POP,
    OP_POPN,

    OP_DEFINE_GLOBAL,
        OP_DEFINE_GLOBAL_LONG,
    OP_GET_GLOBAL,
        OP_GET_GLOBAL_LONG,
    OP_SET_GLOBAL,
        OP_SET_GLOBAL_LONG,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,

    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,

    OP_PRINT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,

    OP_CALL,
    OP_CLOSURE,
        OP_CLOSURE_LONG,
    OP_CLOSE_UPVALUE,
    OP_RETURN,

    OP_CLASS,
        OP_CLASS_LONG,
    OP_GET_PROPERTY,
        OP_GET_PROPERTY_LONG,
    OP_SET_PROPERTY,
        OP_SET_PROPERTY_LONG,
    OP_METHOD,
        OP_METHOD_LONG,
    OP_INVOKE,
        OP_INVOKE_LONG,
    OP_INHERIT,
    OP_GET_SUPER,
        OP_GET_SUPER_LONG,
    OP_SUPER_INVOKE,
        OP_SUPER_INVOKE_LONG
} Opcode;

static inline bool isLongOpcode(Opcode op){
    switch (op){
        case OP_CONSTANT_LONG:
        case OP_DEFINE_GLOBAL_LONG:
        case OP_GET_GLOBAL_LONG:
        case OP_SET_GLOBAL_LONG:
        case OP_CLOSURE_LONG:
        case OP_CLASS_LONG:
        case OP_GET_PROPERTY_LONG:
        case OP_SET_PROPERTY_LONG:
        case OP_METHOD_LONG:
        case OP_INVOKE_LONG:
        case OP_GET_SUPER_LONG:
        case OP_SUPER_INVOKE_LONG:
            return true;
        default:
            return false;
    }
}

typedef struct {
    int offset;
    int line;
} LineStart;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint8_t* code;
    ValueArray constants;
    int lineCount;
    int lineCapacity;
    LineStart* lines;
} Chunk;
    
void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);

void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
int getLine(Chunk* chunk, size_t offset);

#endif