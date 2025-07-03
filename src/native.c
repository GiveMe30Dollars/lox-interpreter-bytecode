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
            case OBJ_ARRAY_SLICE: {
                sentinel = copyString("Slice", 5);
                break;
            }
            default: ;    // Fallthrough
        }
    }
    if (sentinel != NULL){
        // the sentinels must be in the STL.
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

// NATIVE METHOD FAILURES
static inline void writeException(Value* args, Value payload){
    args[-1] = payload;
    args[-1] = OBJ_VAL(newException(payload));
}
static inline void indexNotNumber(Value* args){
    ObjString* message = printToString("Index must be a whole number.");
    writeException(args, OBJ_VAL(message));
}
static inline void indexOutOfRange(Value* args){
    ObjString* message = printToString("Index out of range.");
    writeException(args, OBJ_VAL(message));
}

// HELPERS
static inline bool isWholeNumber(Value value){
    return IS_NUMBER(value) && (floor(AS_NUMBER(value)) == AS_NUMBER(value));
}


// STRING SENTINEL METHODS

// defines idx as an integer if the value given is a whole number within the bounds of the array given.
// otherwise, does error logging and returns -1 to signal an error.
static int stringCheckIndex(Value* args, ObjString* str, Value target){
    if (!IS_NUMBER(target) || floor(AS_NUMBER(target)) != AS_NUMBER(target)) {
        indexNotNumber(args);
        return -1;
    }
    int idx = (int)AS_NUMBER(target);
    if (idx < 0) idx += str->length;
    if (idx >= str->length || idx < 0){
        indexOutOfRange(args);
        return -1;
    }
    return idx;
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
                return OBJ_VAL(copyString("<Array object>", 14));
            case OBJ_ARRAY_SLICE: {
                ObjArraySlice* slice = AS_ARRAY_SLICE(args[0]);
                #define SLICE_COMPONENT(field) \
                    AS_CSTRING(stringPrimitiveNative(1, &field))
                return OBJ_VAL(printToString("Slice: %s, %s, %s", 
                    SLICE_COMPONENT(slice->start), SLICE_COMPONENT(slice->end), SLICE_COMPONENT(slice->step) ));
                #undef SLICE_COMPONENT
            }
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
        // rearrange for call signature of toString().
        args[-1] = args[0];
        pop();
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
    if (IS_NUMBER(args[0])){
        int idx = stringCheckIndex(args, str, args[0]);
        return OBJ_VAL(copyString(&str->chars[idx], 1));
    }
    else if (IS_ARRAY_SLICE(args[0])){
        ObjArraySlice* slice = AS_ARRAY_SLICE(args[0]);
        int step = (int)AS_NUMBER(slice->step);
        int start;
        if (IS_NIL(slice->start)){
            start = step > 0 ? 0 : str->length - 1;
        } else if ((start = stringCheckIndex(args, str, slice->start)) == -1){
            return EMPTY_VAL();
            // if start is assigned to and is not equal to error -1, then proceed
        }
        int end;
        if (IS_NIL(slice->end)){
            end = step > 0 ? str->length : -1;
        } else if ((end = stringCheckIndex(args, str, slice->end)) == -1){
            return EMPTY_VAL();
            // if end is assigned to and is not equal to error -1, then proceed
        }

        int bufferLength = (end - start) / step;
        char* buffer = ALLOCATE(char, bufferLength + 1);
        int j = 0;
        for (int i = start; (step > 0 ? i < end : i > end); i += step){
            buffer[j++] = str->chars[i];
        }
        buffer[bufferLength] = '\0';
        return OBJ_VAL(takeString(buffer, bufferLength));
    }
    else {
        ObjString* payload = printToString("Expected an integer index or a slice object.");
        writeException(args, OBJ_VAL(payload));
        return EMPTY_VAL();
    }
}
Value stringSetNative(int argCount, Value* args){
    ObjString* payload = printToString("Strings are immutable. Consider constructing an array using 'Array(str)' for editing.");
    writeException(args, OBJ_VAL(payload));
    return EMPTY_VAL();
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
Value exceptionPayloadNative(int argCount, Value* args){
    return AS_EXCEPTION(args[-1])->payload;
}

// ARRAY SENTINEL METHODS

// defines idx as an integer if the value given is a whole number within the bounds of the array given.
// otherwise, does error logging and returns -1 to signal an error.
static int arrayCheckIndex(Value* args, ObjArray* array, Value target){
    if (!IS_NUMBER(target) || floor(AS_NUMBER(target)) != AS_NUMBER(target)) {
        indexNotNumber(args);
        return -1;
    }
    int idx = (int)AS_NUMBER(target);
    if (idx < 0) idx += array->data.count;
    if (idx >= array->data.count || idx < 0){
        indexOutOfRange(args);
        return -1;
    }
    return idx;
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
        args[-1] = OBJ_VAL(newException(args[-1]));
        return EMPTY_VAL();
    }
    if (IS_NUMBER(args[0])){
        ObjArray* array = newArray();
        args[-1] = OBJ_VAL(array);
        for (int i = 0; i < AS_NUMBER(args[0]); i++){
            writeValueArray(&array->data, NIL_VAL());
        }
        return args[-1];
    }
    if (IS_STRING(args[0])){
        ObjString* str = AS_STRING(args[0]);
        ObjArray* array = newArray();
        args[-1] = OBJ_VAL(array);

        while (array->data.capacity < str->length)
            array->data.capacity = GROW_CAPACITY(array->data.capacity);
        array->data.values = GROW_ARRAY(Value, array->data.values, 0, array->data.capacity);

        for (int i = 0; i < str->length; i++)
            writeValueArray(&array->data, OBJ_VAL(copyString(str->chars + i, 1)));

        return args[-1];
    }
    return EMPTY_VAL();
}
Value arrayGetNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    if (IS_NUMBER(args[0])){
        int idx = arrayCheckIndex(args, array, args[0]);
        if (idx == -1) return EMPTY_VAL();
        return array->data.values[idx];
    }
    else if (IS_ARRAY_SLICE(args[0])){
        // array slicing get: parse slicing information
        ObjArraySlice* slice = AS_ARRAY_SLICE(args[0]);
        int step = (int)AS_NUMBER(slice->step);
        int start;
        if (IS_NIL(slice->start)){
            start = step > 0 ? 0 : array->data.count;
        } else if ((start = arrayCheckIndex(args, array, slice->start)) == -1){
            return EMPTY_VAL();
            // if start is assigned to and is not equal to error -1, then proceed
        }
        int end;
        if (IS_NIL(slice->end)){
            end = step > 0 ? array->data.count : -1;
        } else if ((end = arrayCheckIndex(args, array, slice->end)) == -1){
            return EMPTY_VAL();
            // if end is assigned to and is not equal to error -1, then proceed
        }

        // args[0] is now available as a local slot for the new array to anchor to the VM
        ObjArray* result = newArray();
        args[0] = OBJ_VAL(result);
        for (int i = start; (step > 0 ? (i < end) : (i > end)); i += step){
            writeValueArray(&result->data, array->data.values[i]);
        }
        return args[0];
    }
    else {
        ObjString* payload = printToString("Expected an integer index or a slice object.");
        writeException(args, OBJ_VAL(payload));
        return EMPTY_VAL();
    }
}
Value arraySetNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    if (IS_NUMBER(args[0])){
        int idx = arrayCheckIndex(args, array, args[0]);
        if (idx == -1) return EMPTY_VAL();
        array->data.values[idx] = args[1];
        return args[1];
    }
    else if (IS_ARRAY_SLICE(args[0])){
        // ensure object to set is an array
        if (!IS_ARRAY(args[1])){
            ObjString* payload = printToString("Expected array object for setting the values of an array slice.");
            writeException(args, OBJ_VAL(payload));
            return EMPTY_VAL();
        }
        ObjArray* source = AS_ARRAY(args[1]);
        
        // parse slicing information
        ObjArraySlice* slice = AS_ARRAY_SLICE(args[0]);
        int step = (int)AS_NUMBER(slice->step);
        int start;
        if (IS_NIL(slice->start)){
            start = step > 0 ? 0 : array->data.count;
        } else if ((start = arrayCheckIndex(args, array, slice->start)) == -1){
            return EMPTY_VAL();
            // if start is assigned to and is not equal to error -1, then proceed
        }
        int end;
        if (IS_NIL(slice->end)){
            end = step > 0 ? array->data.count : -1;
        } else if ((end = arrayCheckIndex(args, array, slice->end)) == -1){
            return EMPTY_VAL();
            // if end is assigned to and is not equal to error -1, then proceed
        }

        int sliceLength = (end - start) / step;
        if (sliceLength != source->data.count){
            ObjString* payload = printToString("Expected array of length %d for this slice but got length %d.", sliceLength, source->data.count);
            writeException(args, OBJ_VAL(payload));
            return EMPTY_VAL();
        }

        int j = 0;
        for (int i = start; (step > 0 ? (i < end) : (i > end)); i += step){
            array->data.values[i] = source->data.values[j++];
        }
        return args[1];
    }
    else {
        ObjString* payload = printToString("Expected an integer index or a slice object.");
        writeException(args, OBJ_VAL(payload));
        return EMPTY_VAL();
    }
}
Value arrayAppendNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    writeValueArray(&array->data, args[0]);
    return NIL_VAL();
}
Value arrayInsertNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);

    // have to handle index checking ourselves because arr[length] is a valid index to insert in
    Value target = args[0];
    if (!IS_NUMBER(target) || floor(AS_NUMBER(target)) != AS_NUMBER(target)) {
        indexNotNumber(args);
        return EMPTY_VAL();
    }
    int idx = (int)AS_NUMBER(target);
    if (idx < 0) idx += array->data.count;
    // the change is here!
    if (idx > array->data.count || idx < 0){
        indexOutOfRange(args);
        return -1;
    }

    if (idx == array->data.count){
        // equivalent to appending
        writeValueArray(&array->data, args[1]);
        return NIL_VAL();
    } else {
        // memmove subsequent elements one step away, then add
        writeValueArray(&array->data, NIL_VAL());
        memmove(&array->data.values[idx + 1], &array->data.values[idx], (array->data.count - idx - 1) * sizeof(Value));
        array->data.values[idx] = args[1];
        return NIL_VAL();
    }
}
Value arrayDeleteNative(int argCount, Value* args){
    ObjArray* array = AS_ARRAY(args[-1]);
    int idx = arrayCheckIndex(args, array, args[0]);
    if (idx == -1) return EMPTY_VAL();

    if (idx == array->data.count - 1){
        // tail: simply decrement count
        array->data.count--;
        return NIL_VAL();
    }
    // non-tail: memmove subsequent elements to this position
    array->data.count--;
    memmove(&array->data.values[idx], &array->data.values[idx + 1], (array->data.count - idx) * sizeof(Value));
    return NIL_VAL();
}
Value arrayLengthNative(int argCount, Value* args){
    return NUMBER_VAL(AS_ARRAY(args[-1])->data.count);
}


// SLICE SENTINEL METHODS

Value sliceInitNative(int argCount, Value* args){
    // check that arguments are valid
    for (int i = 0; i < 3; i++){
        if (IS_NIL(args[i])){
            // nil is accepted for start and end, is converted to 1 for step
            if (i == 2) args[2] = NUMBER_VAL(1);
            continue;
        } else if (isWholeNumber(args[i])){
            if ((i == 2) && (AS_NUMBER(args[2]) == 0)){
                ObjString* payload = printToString("Step size must be non-zero.");
                writeException(args, OBJ_VAL(payload));
                return EMPTY_VAL();
            } else continue;
        } else {
            // Argument is something other than a whole number or nil
            ObjString* payload = printToString("Argument '%d' must be either a whole number or 'nil'.", i);
            writeException(args, OBJ_VAL(payload));
            return EMPTY_VAL();
        }
    }
    args[-1] = OBJ_VAL(newSlice(args[0], args[1], args[2]));
    return args[-1];
}
Value sliceRawNative(int argCount, Value* args){
    switch (argCount){
        case 1: return args[0];
        case 2: {
            push(NIL_VAL());
            Value result = sliceInitNative(3, args);
            pop();
            return result;
        }
        case 3: return sliceInitNative(3, args);
        default:  // Unreachable
            return EMPTY_VAL();
    }
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
            IMPORT_NATIVE("set", stringSetNative, 2),
            IMPORT_NATIVE("length", stringLengthNative, 0),

        IMPORT_SENTINEL("Function", 1),
            IMPORT_NATIVE("arity", functionArityNative, 0),

        IMPORT_SENTINEL("Exception", 2),
            IMPORT_NATIVE("init", exceptionInitNative, 1),
            IMPORT_NATIVE("payload", exceptionPayloadNative, 0),

        IMPORT_SENTINEL("Array", 8),
            IMPORT_STATIC("@raw", arrayRawNative, -1),
            IMPORT_NATIVE("init", arrayInitNative, -1),
            IMPORT_NATIVE("get", arrayGetNative, 1),
            IMPORT_NATIVE("set", arraySetNative, 2),
            IMPORT_NATIVE("append", arrayAppendNative, 1),
            IMPORT_NATIVE("insert", arrayInsertNative, 2),
            IMPORT_NATIVE("delete", arrayDeleteNative, 1),
            IMPORT_NATIVE("length", arrayLengthNative, 0),

        IMPORT_SENTINEL("Slice", 2),
            IMPORT_STATIC("@raw", sliceRawNative, -1),
            IMPORT_NATIVE("init", sliceInitNative, 3),
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