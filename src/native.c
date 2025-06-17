#include <time.h>
#include <stdio.h>
#include <string.h>

#include "native.h"
#include "memory.h"


Value clockNative(int argCount, Value* args){
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
Value stringNative(int argCount, Value* args){
    Value value = args[0];
    #ifdef VALUE_NAN_BOXING
    if (IS_EMPTY(value)){
        return OBJ_VAL(copyString("<empty>", 7));
    } else if (IS_NIL(value)){
        return OBJ_VAL(copyString("nil", 3));
    } else if (IS_BOOL(value)){
        return OBJ_VAL( AS_BOOL(value) ? (Obj*)copyString("true", 4) : (Obj*)copyString("false", 5) );
    } else if (IS_NUMBER(value)){
        double number = AS_NUMBER(value);
        int length = snprintf(NULL, 0, "%g", number);
        char* buffer = ALLOCATE(char, length + 1);
        snprintf(buffer, length + 1, "%g", number);
        return OBJ_VAL(takeString(buffer, length));
    } else {
        switch(OBJ_TYPE(value)){
            case OBJ_STRING:
                return value;
            case OBJ_FUNCTION:
                return OBJ_VAL(AS_FUNCTION(value)->name);
            case OBJ_NATIVE: 
                return OBJ_VAL(AS_NATIVE(value)->name);
            case OBJ_CLOSURE:
                return OBJ_VAL(AS_CLOSURE(value)->function->name);
            case OBJ_CLASS:
                return OBJ_VAL(AS_CLASS(value)->name);
            case OBJ_INSTANCE:
                return OBJ_VAL(AS_INSTANCE(value)->klass->name);
        }
    }
    return OBJ_VAL(copyString("", 0));
    #else
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
                case OBJ_NATIVE: 
                    return OBJ_VAL(AS_NATIVE(value)->name);
                case OBJ_CLOSURE:
                    return OBJ_VAL(AS_CLOSURE(value)->function->name);
                case OBJ_CLASS:
                    return OBJ_VAL(AS_CLASS(value)->name);
                case OBJ_INSTANCE:
                    return OBJ_VAL(AS_INSTANCE(value)->klass->name);
            }
        }
    }
    // Default case: empty string.
    return OBJ_VAL(copyString("", 0));
    #endif
}
Value concatenateNative(int argCount, Value* args){
    // Concatenates any number of strings, up to UINT8_MAX
    // If no arguments passed, return empty string
    if (argCount == 0) return OBJ_VAL(copyString("", 0));
    int length = 0;
    for (int i = 0; i < argCount; i++)
        length += AS_STRING(args[i])->length;
    
    char* chars = ALLOCATE(char, length + 1);
    int offset = 0;
    for (int i = 0; i < argCount; i++){
        ObjString* str = AS_STRING(args[i]);
        memcpy(chars + offset, str->chars, str->length);
        offset += str->length;
    }
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    return(OBJ_VAL(result));
}

ImportNative importLibrary[] = {
    {"clock", clockNative, 0},
    {"string", stringNative, 1},
    {"concatenate", concatenateNative, -1}
};

size_t importCount = sizeof(importLibrary) / sizeof(ImportNative);