#ifndef clox_compiler_h
#define clox_compiler_h

#include "scanner.h"
#include "vm.h"

// Single-pass compiler
// States are hidden in private functions
ObjFunction* compile(const char* source, bool evalExpr);

void markCompilerRoots();

#endif