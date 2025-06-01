#include "debug.h"

// PRIVATE HELPER FUNCTIONS

static int simpleInstruction(const char* name, int offset){
    printf("%s\n", name);
    return offset + 1;
}
static int constantInstruction(const char* name, Chunk* chunk, int offset){
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}


// IMPLEMETATION OF PUBLIC FUNCTIONS

void disassembleChunk(Chunk* chunk, const char* name){
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count; ){
        // offset is incremented in disassembleInstruction
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk* chunk, int offset){
    // Offset information
    printf("%04d ", offset);

    // Line information
    if (offset > 0 && getLine(&chunk->lineInfo, offset) == getLine(&chunk->lineInfo, offset - 1)){
        printf("   | ");
    } else {
        printf("%4d ", getLine(&chunk->lineInfo, offset));
    }

    // Opcode information
    uint8_t instruction = chunk->code[offset];
    switch (instruction){
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
    }
}
