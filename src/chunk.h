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

    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,

    OP_NOT,
    OP_NEGATE,
    OP_RETURN
} OpCode;

typedef struct {
    int count;
    int capacity;
    int* raw;
} LineInfo;

void initLineInfo(LineInfo* lineInfo);
void addLine(LineInfo* lineInfo, int line);
int getLine(LineInfo* lineInfo, int offset);
void freeLineInfo(LineInfo* lineInfo);

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    LineInfo lineInfo;
    ValueArray constants;
} Chunk;
    
void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
void freeChunk(Chunk* chunk);

#endif