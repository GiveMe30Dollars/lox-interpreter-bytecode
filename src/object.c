#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"
#include "hashtable.h"


// GENERAL OBJECT ALLOCATION METHODS

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type){
    // allocate and assign object type
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    // insert new object into VM linked list
    object->next = vm.objects;
    vm.objects = object;

    #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
    #endif

    return object;
}


// OBJSTRING METHODS

static ObjString* allocateString(char* chars, int length, uint32_t hash){
    // allocate, intern and return a new ObjString
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    push(OBJ_VAL(string));
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL());
    pop();
    return string;
}
ObjString* copyString(const char* start, int length){
    // creates a deep copy of C-substring with terminator (+ '\0')
    // encapsulate in ObjString*

    uint32_t hash = HASH_CSTRING(start, length);
    ObjString* interned = tableFindString(&vm.strings, start, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, start, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}
ObjString* takeString(char* start, int length){
    // takes the pointer to a C-string
    // encapsulate in ObjString*
    // (string must be solely owned by created ObjString* !)

    uint32_t hash = HASH_CSTRING(start, length);
    ObjString* interned = tableFindString(&vm.strings, start, length, hash);
    if (interned != NULL){
        FREE_ARRAY(char, start, length + 1);
        return interned;
    }
    return allocateString(start, length, hash);
}

ObjString* lambdaString(){
    // we keep a 32-bit hash in vm.hash
    // when we generate a lambda string, regenerate another hash based on that hash
    // then return that as an ObjString*
    // Display name is "[XXXXXXXX]" of length 10 (+1 for \0);

    const int length = 10;
    char* buffer = ALLOCATE(char, length + 1);
    do {
        vm.hash = hashBytes((uint8_t*)&vm.hash, 4);
        snprintf(buffer, length + 1, "[%08x]", vm.hash);
    } while (tableFindString(&vm.strings, buffer, length, vm.hash) != NULL);
    // this ensures no anonymous function will have the same hash
    // realistically, FNV-1a is good enough that this won't repeat

    return takeString(buffer, length);
}

// OBJUPVALUE METHODS

ObjUpvalue* newUpvalue(Value* location){
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = location;
    upvalue->closed = NIL_VAL();
    upvalue->next = NULL;
    return upvalue;
}


// OBJFUNCTION METHODS

ObjFunction* newFunction(){
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

// OBJFUNCTION METHODS

ObjNative* newNative(NativeFn function, int arity, ObjString* name){
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    native->name = name;
    return native;
}

// OBJCLOSURE METHODS

ObjClosure* newClosure(ObjFunction* function){
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++){
        upvalues[i] = NULL;
    }
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    closure->function = function;
    return closure;
}

// OBJECT GENERAL METHODS

void printFunction(ObjFunction* function){
    if (function->name == NULL){
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}
void printObject(Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value)); break;
        case OBJ_UPVALUE:
            printf("<upvalue>"); break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value)); break;
        case OBJ_NATIVE:
            printf("<fn %s>", AS_NATIVE(value)->name->chars); break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function); break;
        default: break;    // Unreachable.
    }
}