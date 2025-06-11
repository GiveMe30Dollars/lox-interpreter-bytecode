#include "native.h"
#include "memory.h"

#include <time.h>
#include <stdio.h>

Value clockNative(int argCount, Value* args){
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
Value stringNative(int argCount, Value* args){
    Value value = args[0];
    switch(value.type){
        case VAL_NIL:
            return OBJ_VAL(copyString("nil", 3));
        case VAL_BOOL:
            return OBJ_VAL( AS_BOOL(value) ? (Obj*)copyString("true", 4) : (Obj*)copyString("false", 5) );
        case VAL_EMPTY:
            return OBJ_VAL(copyString("<empty>", 7));
        case VAL_NUMBER: {
            double number = AS_NUMBER(value);
            int length = snprintf(NULL, 0, "%g", number);
            char* buffer = ALLOCATE(char, length + 1);
            snprintf(buffer, length + 1, "%g", number);
            return OBJ_VAL(takeString(buffer, length));
        }
        case VAL_OBJ: {
            switch(OBJ_TYPE(value)){
                case OBJ_STRING:
                    return value;
                case OBJ_FUNCTION:
                    return OBJ_VAL(AS_FUNCTION(value)->name);
                case OBJ_NATIVE: {
                    return OBJ_VAL(AS_NATIVE(value)->name);
                }
            }
        }
    }
    return EMPTY_VAL();
}

ImportNative importLibrary[] = {
    {"clock", clockNative, 0},
    {"string", stringNative, 1}
};

size_t importCount = sizeof(importLibrary) / sizeof(ImportNative);