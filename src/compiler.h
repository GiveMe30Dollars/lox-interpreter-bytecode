#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"

// Single-pass compiler
// States are hidden in private functions

bool compile(const char* source, Chunk* chunk);

#endif