#ifndef clox_native_h
#define clox_native_h

#include "object.h"
#include "value.h"

typedef enum {
    IMPORT_NATIVE,
    IMPORT_SYNTH,
    IMPORT_STATIC
} ImportHeader;

typedef struct {
    const char* name;
    NativeFn function;
    int arity;
} ImportNative;

typedef struct {
    const char* name;
    const int numOfMethods;
} ImportSynth;

typedef struct {
    ImportHeader header;
    union {
        ImportNative native;
        ImportSynth synth;
    } as;
} ImportStruct;

typedef struct {
    size_t count;
    ImportStruct* start;
} ImportInfo;

#define IMPORT_NATIVE(name, function, arity) \
    ((ImportStruct){IMPORT_NATIVE, {.native = {name, function, arity}}})
#define IMPORT_STATIC(name, function, arity) \
    ((ImportStruct){IMPORT_STATIC, {.native = {name, function, arity}}})
#define IMPORT_SYNTH(name, num) \
    ((ImportStruct){IMPORT_SYNTH, {.synth = {name, num}}})


ImportInfo buildSTL();
void freeSTL(ImportInfo library);

// NATIVE FUNCTIONS AND METHODS

Value clockNative(int argCount, Value* args);

Value typeNative(int argCount, Value* args);
Value hasMethodNative(int argCount, Value* args);

Value stringPrimitiveNative(int argCount, Value* args);

#endif