#include <stdio.h>
#include <stdlib.h>

#include "io.h"

char* readFile(const char* path){
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