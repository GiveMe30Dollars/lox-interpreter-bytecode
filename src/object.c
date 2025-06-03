#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"


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

static ObjString* allocateString(char* chars, int length){
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}
ObjString* copyString(char* start, int length){
    // creates a deep copy of C-substring with terminator (+ '\0')
    // encapsulate in ObjString*
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, start, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}
ObjString* takeString(char* start, int length){
    // takes the pointer to a C-string
    // encapsulate in ObjString*
    // (string must be owned by created ObjString* !)
    return allocateString(start, length);
}


// OBJECT GENERAL METHODS

void printObject(Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value)); break;
        default: break;    // Unreachable.
    }
}