#include "common.h"
#include "chunk.h"
#include "vm.h"

#include "debug.h"

int main(int argc, const char *argv[]) {
    initVM();
    Chunk chunk;
    initChunk(&chunk);

    int constantA = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantA, 123);

    int constantB = addConstant(&chunk, 6);
    writeChunk(&chunk, OP_CONSTANT, 244);
    writeChunk(&chunk, constantB, 244);

    writeChunk(&chunk, OP_RETURN, 300);

    interpret(&chunk);

    disassembleChunk(&chunk, "test chunk");

    freeChunk(&chunk);
    freeVM();

    return 0;
}