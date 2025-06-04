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

    // insert new object into VM linked list
    object->next = vm.objects;
    vm.objects = object;

    return object;
}


// OBJSTRING METHODS

static ObjString* allocateString(char* chars, int length, uint32_t hash){
    // allocate, intern and return a new ObjString
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL());
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


// OBJECT GENERAL METHODS

void printObject(Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value)); break;
        default: break;    // Unreachable.
    }
}