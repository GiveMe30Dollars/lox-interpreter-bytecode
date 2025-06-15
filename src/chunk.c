#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"


void initChunk(Chunk* chunk){
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initValueArray(&chunk->constants);
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;
}
void freeChunk(Chunk* chunk){
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int,  chunk->lines, chunk->lineCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line){
    // writes a byte to the chunk
    if (chunk->count + 1 > chunk->capacity){
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;

    // writes line information
    if (chunk->lineCount > 0 && line == chunk->lines[chunk->lineCount-1].line){
        return;
    }
    if (chunk->lineCount + 1 > chunk->lineCapacity){
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(chunk->lineCapacity);
        chunk->lines = GROW_ARRAY(LineStart, chunk->lines, oldCapacity, chunk->lineCapacity);
    }
    LineStart* lineStart = &chunk->lines[chunk->lineCount++];
    // Account for increment of chunk->count
    lineStart->offset = chunk->count - 1;
    lineStart->line = line;
}
int addConstant(Chunk* chunk, Value value){
    // writes a constant to the constants array
    // returns its array index
    push(value);
    writeValueArray(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
}
int getLine(Chunk* chunk, size_t instruction){
    int start = 0;
    int end = chunk->lineCount - 1;
    // binary search (start, end inclusive)
    for (;;){
        int mid = (start + end) / 2;
        LineStart* line = &chunk->lines[mid];
        if (instruction < line->offset){
            end = mid - 1;
        } else if (mid == chunk->lineCount - 1 || instruction < chunk->lines[mid + 1].offset){
            return line->line;
        } else {
            start = mid + 1;
        }
    }
}