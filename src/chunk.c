#include <stdlib.h>
#include "chunk.h"
#include "memory.h"

void initLineInfo(LineInfo* info){
    // raw info interleaves line number and runlength in info (num, length, num, length, etc.)
    info->count = 0;
    info->capacity = 0;
    info->raw = NULL;
}
void addLine(LineInfo* info, int line){
    // if run of current line exists in raw
    // increment runlength
    if (info->count > 0 && info->raw[info->count - 2] == line){
        info->raw[info->count - 1]++;
        return;
    }
    // add new run for this line into raw
    // grow capacity of raw if necessary
    if (info->count + 2 > info->capacity){
        int oldCapacity = info->capacity;
        info->capacity = GROW_CAPACITY(oldCapacity);
        info->raw = GROW_ARRAY(int, info->raw, oldCapacity, info->capacity);
    }
    info->raw[info->count] = line;
    info->raw[info->count + 1] = 1;
    info->count += 2;
}
int getLine(LineInfo* info, int offset){
    // for every run, return the line number if the total offset is less than offset (offset is in current run)
    int totalOffset = 0;
    for (int i = 0; i < info->count; i += 2){
        totalOffset += info->raw[i + 1];
        if (offset < totalOffset) return info->raw[i];
    }
}
void freeLineInfo(LineInfo* info){
    FREE_ARRAY(int, info->raw, info->capacity);
    initLineInfo(info);
}


void initChunk(Chunk* chunk){
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initLineInfo(&chunk->lineInfo);
    initValueArray(&chunk->constants);
}
void writeChunk(Chunk* chunk, uint8_t byte, int line){
    // writes a byte to the chunk
    if (chunk->count + 1 > chunk->capacity){
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    addLine(&chunk->lineInfo, line);
    chunk->count++;
}
int addConstant(Chunk* chunk, Value value){
    // writes a constant to the constants array
    // returns its array index
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void freeChunk(Chunk* chunk){
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeLineInfo(&chunk->lineInfo);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}