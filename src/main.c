#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "io.h"
#include "vm.h"

static void repl(){
    char line[1024];
    for (;;){
        printf(">>> ");
        if (!fgets(line, sizeof(line), stdin)){
            printf("\n");
            break;
        }
        if (memcmp(line, "exit\n", 5) == 0) break;
        if (memcmp(line, "reset\n", 5) == 0){
            freeVM();
            initVM();
            continue;
        }
        interpret(line, true);
    }
}

static void runFile(const char* path){
    char* source = readFile(path);
    interpret(source, false);
}

int main(int argc, const char *argv[]) {
    
    initVM();
    if (argc == 1){
        repl();
    } else if (argc == 2){
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: ./lox.sh [path]\n");
        fprintf(stderr, "    |  ./lox.sh       \n");
        exit(1);
    }
    freeVM();

    return 0;
}