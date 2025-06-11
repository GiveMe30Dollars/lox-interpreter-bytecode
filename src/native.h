#ifndef clox_native_h
#define clox_native_h

#include "object.h"
#include "value.h"

typedef struct {
    const char* name;
    NativeFn function;
    int arity;
} ImportNative;

extern ImportNative importLibrary[];
extern size_t importCount;


Value clockNative(int argCount, Value* args);
Value stringNative(int argCount, Value* args);
Value concatenateNative(int argCount, Value* args);

#endif