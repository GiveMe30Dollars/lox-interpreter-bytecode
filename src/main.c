#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_CONSTANT, 244);
    writeChunk(&chunk, constant, 244);
    writeChunk(&chunk, OP_RETURN, 244);

    disassembleChunk(&chunk, "test chunk");
    for (int i = 0; i < chunk.lineInfo.count; i += 2){
        printf("%d : %d\n", chunk.lineInfo.raw[i], chunk.lineInfo.raw[i+1]);
    }

    freeChunk(&chunk);
    return 0;
}