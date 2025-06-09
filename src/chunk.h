#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// Contains all declarations regarding the Chunk class
// as well as the enum OpCode for bytecode

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_POPN,

    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,

    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,

    OP_NOT,
    OP_NEGATE,
    OP_UNARY_PLUS,

    OP_PRINT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    
    OP_RETURN
} Opcode;

typedef struct {
    int offset;
    int line;
} LineStart;

typedef struct {
    int count;
    int capacity;
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
int getLine(Chunk* chunk, int offset);

#endif