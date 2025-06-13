#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void* reallocate(void* ptr, size_t oldSize, size_t newSize){
    if (newSize == 0){
        free(ptr);
        return NULL;
    }
    void* result = realloc(ptr, newSize);
    if (result == NULL) exit(1);
    return result;
}


void freeObject(Obj* object){
    switch(object->type){
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length);
            FREE(ObjString, string);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, function);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_CLOSURE: {
            // do not free underlying ObjFunction
            // free upvalues ARRAY but not the upvalues themselves
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
    }
}
void freeObjects(){
    Obj* object = vm.objects;
    while (object != NULL){
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}