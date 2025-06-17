#include <stdio.h>
#include <string.h>
#include <math.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void printValue(Value value){
    #ifdef VALUE_NAN_BOXING
    if (IS_EMPTY(value)){
        printf("<empty>");
    } else if (IS_NIL(value)){
        printf("nil");
    } else if (IS_BOOL(value)){
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NUMBER(value)){
        printf("%g", AS_NUMBER(value));
    } else {
        // Object
        printObject(value);
    }
    #else
    switch(value.type){
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL:
            printf("nil"); break;
        case VAL_NUMBER:{
            printf("%g", AS_NUMBER(value));
            break;
        }
        case VAL_EMPTY:
            printf("<empty>"); break;
        case VAL_OBJ:
            printObject(value); break;

        default: return;    // Unreachable.
    }
    #endif
}
bool valuesEqual(Value a, Value b){
    #ifdef VALUE_NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b)){
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a == b;
    #else
    if (a.type != b.type) return false;
    switch(a.type){
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:{
            // for ObjString, directly compare as all strings are interned
            return AS_OBJ(a) == AS_OBJ(b);
        }
        case VAL_EMPTY:  return true;
        default: return false;    // Unreachable.
    }
    #endif
}

void initValueArray(ValueArray* array){
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}
void freeValueArray(ValueArray* array){
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void writeValueArray(ValueArray* array, Value value){
    if (array->count + 1 > array->capacity){
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}