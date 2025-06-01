#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "vm.h"

#include "debug.h"

static void repl(){
    char line[1024];
    for (;;){
        printf(">>> ");
        if (!fgets(line, sizeof(line), stdin)){
            printf("\n");
            break;
        }
        if (memcmp(line, "exit\n", 5) == 0) break;
        interpret(line);
    }
}
static char* readFile(const char* path){
    FILE* file = fopen(path, "rb");
    if (file == NULL){
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    // get size of file by seeking to end, then rewind
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // write to char array
    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL){
        fprintf(stderr, "Not enough memory to read file \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize){
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}
static void runFile(const char* path){
    char* source = readFile(path);
    interpret(source);
}

int main(int argc, const char *argv[]) {
    initVM();

    if (argc == 1){
        repl();
    } else if (argc == 2){
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: ./lox.sh [path]\n");
        exit(1);
    }

    /*
    Chunk chunk;
    initChunk(&chunk);

    int constantA = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantA, 123);

    int constantB = addConstant(&chunk, 6);
    writeChunk(&chunk, OP_CONSTANT, 244);
    writeChunk(&chunk, constantB, 244);

    writeChunk(&chunk, OP_NEGATE, 288);
    writeChunk(&chunk, OP_ADD, 288);

    writeChunk(&chunk, OP_RETURN, 300);

    interpret(&chunk);

    disassembleChunk(&chunk, "test chunk");

    freeChunk(&chunk);
    */
    freeVM();

    return 0;
}