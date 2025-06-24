#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "native.h"
#include "memory.h"
#include "vm.h"


Value clockNative(int argCount, Value* args){
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value typeNative(int argCount, Value* args){
    Value value = args[0];

    // if a sentinel identified, the string is already interned. this is safe.
    ObjString* sentinel = NULL;

    if (IS_NIL(value)){
        return NIL_VAL();
    } else if (IS_BOOL(value)){
        sentinel = copyString("Boolean", 7);
    } else if (IS_NUMBER(value)){
        sentinel = copyString("Number", 6);
    } else {
        switch(OBJ_TYPE(value)){
            case OBJ_STRING: {
                sentinel = copyString("String", 6);
                break;
            }
            case OBJ_NATIVE:
            case OBJ_FUNCTION:
            case OBJ_CLOSURE:
            case OBJ_BOUND_METHOD: {
                sentinel = copyString("Function", 8);
                break;
            }
            case OBJ_CLASS:
                return value;
            case OBJ_INSTANCE:
                return OBJ_VAL(AS_INSTANCE(value)->klass);

            case OBJ_EXCEPTION:{
                sentinel = copyString("Exception", 9);
                break;
            }
            case OBJ_ARRAY: {
                sentinel = copyString("Array", 5);
                break;
            }
            default: ;    // Fallthrough
        }
    }
    if (sentinel != NULL){
        Value klass;
        tableGet(&vm.stl, OBJ_VAL(sentinel), &klass);
        return klass;
    }
    return EMPTY_VAL();
}

Value hasMethodNative(int argCount, Value* args){
    Value klass = typeNative(1, &args[0]);
    Value output;
    if (tableGet(&AS_CLASS(klass)->methods, args[1], &output))
        return output;
    return NIL_VAL();
}


// STRING SENTINEL METHODS

static inline Value indexNotNumber(){
    return OBJ_VAL(copyString("Index must be a whole number.", 29));
}
static inline Value indexOutOfRange(){
    return OBJ_VAL(copyString("Index out of range.", 19));
}


Value stringPrimitiveNative(int argCount, Value* args){
    Value value = args[0];
    if (IS_EMPTY(value)){
        return OBJ_VAL(copyString("<empty>", 7));
    } else if (IS_NIL(value)){
        return OBJ_VAL(copyString("nil", 3));
    } else if (IS_BOOL(value)){
        return OBJ_VAL( AS_BOOL(value) ? copyString("true", 4) : copyString("false", 5) );
    } else if (IS_NUMBER(value)){
        return OBJ_VAL(printToString("%g", AS_NUMBER(value)));
    } else {
        switch(OBJ_TYPE(value)){
            case OBJ_STRING:
                return value;
            case OBJ_FUNCTION:
            case OBJ_NATIVE: 
            case OBJ_CLOSURE:
                return OBJ_VAL(printFunctionToString(value));
            case OBJ_BOUND_METHOD:
                return OBJ_VAL(printFunctionToString( OBJ_VAL(AS_BOUND_METHOD(value)->method) ));

            case OBJ_CLASS:
                return OBJ_VAL(printToString("<class %s>", AS_CLASS(value)->name));
            case OBJ_INSTANCE:
                return OBJ_VAL(printToString("<%s instance>", AS_INSTANCE(value)->klass->name->chars));

            case OBJ_EXCEPTION: {
                Value payload = AS_EXCEPTION(value)->payload;
                Value payloadString = stringPrimitiveNative(1, &payload);
                return OBJ_VAL(printToString("Exception: %s", AS_CSTRING(payloadString)));
            }
            case OBJ_ARRAY:
                return OBJ_VAL(copyString("<Array instance>", 16));
        }
    }
    return OBJ_VAL(copyString("", 0));
}
Value stringNative(int argCount, Value* args){
    // EMPTY_VAL can also signify that native function has passed execution to non-native function
    Value value = args[0];
    Value toStringName = OBJ_VAL(copyString("toString", 8));
    args[1] = toStringName;
    Value hasToString = hasMethodNative(2, args);
    if (!IS_NIL(hasToString)){
        memcpy(&args[-1], &args[0], sizeof(Value));
        invoke(toStringName, 0);
        return EMPTY_VAL();
    }
    return stringPrimitiveNative(argCount, args);
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
Value stringAddNative(int argCount, Value* args){
    Value stringSentinel;
    tableGet(&vm.stl, OBJ_VAL(copyString("String", 6)), &stringSentinel);
    if (typeNative(1, &args[0]) != stringSentinel){
        args[-1] = OBJ_VAL(copyString("Operand not a string.", 21));
        return EMPTY_VAL();
    }
    args[-1] = concatenateNative(argCount + 1, &args[-1]);
    return args[-1];
}
Value stringGetNative(int argCount, Value* args){
    ObjString* str = AS_STRING(args[-1]);
    if (!IS_NUMBER(args[0]) || floor(AS_NUMBER(args[0])) != AS_NUMBER(args[0])) {
        args[-1] = indexNotNumber();
        return EMPTY_VAL();
    }
    int idx = (int)AS_NUMBER(args[0]);
    if (idx >= str->length || idx < -str->length){
        args[-1] = indexOutOfRange();
        return EMPTY_VAL();
    }
    if (idx < 0) idx += str->length;
    return OBJ_VAL(copyString(&str->chars[idx], 1));
}
Value stringLengthNative(int argCount, Value* args){
    return NUMBER_VAL(AS_STRING(args[-1])->length);
}

// FUNCTION SENTINEL METHODS

Value functionArityNative(int argCount, Value* args){
    return NUMBER_VAL(AS_FUNCTION(args[-1])->arity);
}

// EXCEPTION SENTINEL METHODS

Value exceptionInitNative(int argCount, Value* args){
    ObjException* exception = newException(args[0]);
    return OBJ_VAL(exception);
}


// ARRAY SENTINEL METHODS

Value arrayAllocateNative(int argCount, Value* args){
    int num = (int)AS_NUMBER(args[0]);
    ObjArray* array = newArray();
    args[-1] = OBJ_VAL(array);
    for (int i = 0; i < num; i++){
        writeValueArray(&array->data, NIL_VAL());
    }
    return OBJ_VAL(array);
}
Value arrayRawNative(int argCount, Value* args){
    ObjArray* array = newArray();
    args[-1] = OBJ_VAL(array);
    for (int i = 0; i < argCount; i++){
        writeValueArray(&array->data, args[i]);
    }
    return OBJ_VAL(array);
}
Value arrayInitNative(int argCount, Value* args){
    if (argCount == 0) return OBJ_VAL(newArray());
    if (argCount > 2){
        args[-1] = OBJ_VAL(copyString("Expected no more than 1 argument.", 33));
        args[-1] = newException(args[-1]);
        return EMPTY_VAL();
    }
    if (IS_NUMBER(args[0])){
        ObjArray* array = newArray();
        args[-1] = OBJ_VAL(array);
        for (int i = 0; i < args[0]; i++){
            writeValueArray(&array->data, NIL_VAL());
        }
        return args[-1];
    }
    return EMPTY_VAL();
}
Value arrayGetNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    if (!IS_NUMBER(args[0]) || floor(AS_NUMBER(args[0])) != AS_NUMBER(args[0])) {
        args[-1] = indexNotNumber();
        return EMPTY_VAL();
    }
    int idx = (int)AS_NUMBER(args[0]);
    if (idx >= array->data.count || idx < -array->data.count){
        args[-1] = indexOutOfRange();
        return EMPTY_VAL();
    }
    if (idx < 0) idx += array->data.count;
    return array->data.values[idx];
}
Value arraySetNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    if (!IS_NUMBER(args[0]) || floor(AS_NUMBER(args[0])) != AS_NUMBER(args[0])) {
        args[-1] = indexNotNumber();
        return EMPTY_VAL();
    }
    int idx = (int)AS_NUMBER(args[0]);
    if (idx >= array->data.count || idx < -array->data.count){
        args[-1] = indexOutOfRange();
        return EMPTY_VAL();
    }
    if (idx < 0) idx += array->data.count;
    array->data.values[idx] = args[1];
    return args[1];
}
Value arrayAppendNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    writeValueArray(&array->data, args[0]);
    return NIL_VAL();
}
Value arrayLengthNative(int argCount, Value* args){
    return NUMBER_VAL(AS_ARRAY(args[-1])->data.count);
}


// STL BUILD AND FREE

ImportInfo buildSTL(){

    const ImportStruct lib[] = {
        IMPORT_NATIVE("clock", clockNative, 0),

        IMPORT_NATIVE("type", typeNative, 1),
        IMPORT_NATIVE("hasMethod", hasMethodNative, 2),

        IMPORT_SENTINEL("Boolean", 0),
        IMPORT_SENTINEL("Number", 0),

        IMPORT_SENTINEL("String", 5),
            IMPORT_STATIC("init", stringNative, 1),
            IMPORT_STATIC("concatenate", concatenateNative, -1),
            IMPORT_NATIVE("add", stringAddNative, 1),
            IMPORT_NATIVE("get", stringGetNative, 1),
            IMPORT_NATIVE("length", stringLengthNative, 0),

        IMPORT_SENTINEL("Function", 1),
            IMPORT_NATIVE("arity", functionArityNative, 0),

        IMPORT_SENTINEL("Exception", 1),
            IMPORT_NATIVE("init", exceptionInitNative, 1),
        IMPORT_SENTINEL("Array", 6),
            IMPORT_STATIC("@raw", arrayRawNative, -1),
            IMPORT_NATIVE("init", arrayInitNative, -1),
            IMPORT_NATIVE("get", arrayGetNative, 1),
            IMPORT_NATIVE("set", arraySetNative, 2),
            IMPORT_NATIVE("append", arrayAppendNative, 1),
            IMPORT_NATIVE("length", arrayLengthNative, 0)
    };

    ImportStruct* library = malloc(sizeof(lib));
    if (library == NULL) {
        fprintf(stderr, "Allocation for STL failed.\n");
        exit(74);
    }
    memcpy(library, &lib, sizeof(lib));
    size_t count = sizeof(lib) / sizeof(ImportStruct);
    return (ImportInfo){count, library};
}

void freeSTL(ImportInfo library){
    free(library.start);
}