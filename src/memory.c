#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROWTH_FACTOR 2


void* reallocate(void* ptr, size_t oldSize, size_t newSize){
    vm.bytesAllocated += (newSize - oldSize);
    
    if (newSize > oldSize){
        #ifdef DEBUG_STRESS_GC
        collectGarbage();
        #endif
        if (vm.bytesAllocated > vm.nextGC) 
            collectGarbage();
    }

    if (newSize == 0){
        free(ptr);
        return NULL;
    }
    void* result = realloc(ptr, newSize);
    if (result == NULL) exit(1);
    return result;
}


void freeObject(Obj* object){
    #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, (int)objType(object));
    #endif

    switch(objType(object)){
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
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_BOUND_METHOD: {
            FREE(ObjBoundMethod, object);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            freeValueArray(&array->data);
            FREE(ObjArray, object);
            break;
        }
    }
}


void freeObjects(){
    Obj* object = vm.objects;
    while (object != NULL){
        Obj* next = objNext(object);
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}

// GARBAGE COLLECTION METHODS

void markObject(Obj* object){
    if (object == NULL) return;
    if (isMarked(object)) return;

    #ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printObject(OBJ_VAL(object));
    printf("\n");
    #endif

    setIsMarked(object, true);
    // add to gray stack
    if (vm.grayCount + 1 > vm.grayCapacity){
        // expand allocation. exit if reallocation failed.
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL){
            exit(1);
        }
    }
    vm.grayStack[vm.grayCount++] = object;
}
void markValue(Value value){
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}
static void markArray(ValueArray* array){
    for (int i = 0; i < array->count; i++){
        markValue(array->values[i]);
    }
}

static void markRoots(){
    // mark all in-use slots of the stack
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++){
        markValue(*slot);
    }

    // mark all call-frame functions/closures
    for (int i = 0; i < vm.frameCount; i++){
        markObject(vm.frames[i].function);
    }

    // mark all open upvalues
    // (closed upvalues are marked when we traverse the closure objects)
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next){
        markObject((Obj*)upvalue);
    }
    
    // mark all global variables and STL
    markTable(&vm.stl);
    markTable(&vm.globals);
    markValue(vm.initString);

    // mark compiler roots
    markCompilerRoots();
}

static void blackenObject(Obj* object){
    #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
    #endif

    switch(objType(object)){
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;    // Nothing additional to mark
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);    // Mark closed field
            break;
        case OBJ_FUNCTION: {
            // mark name, constants used
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_CLOSURE: {
            // mark associated function and upvalues
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++){
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject((Obj*)klass->name);
            markTable(&klass->methods);
            markTable(&klass->statics);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->receiver);
            markObject(bound->method);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            markArray(&array->data);
            break;
        }
    }
}
static void traceReferences(){
    while (vm.grayCount > 0){
        // pop one object from the stack, trace out all its contents
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

void sweep(){
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (isMarked(object)){
            // reset isMarked flag for next collection
            setIsMarked(object, false);
            previous = object;
            object = objNext(object);
        } else {
            // to be deleted.
            Obj* unreached = object;
            object = objNext(object);
            // relink linked list
            if (previous != NULL) {
                setObjNext(previous, object);
            } else {
                vm.objects = object;
            }
            freeObject(unreached);
        }
    }
}

#ifdef DEBUG_LOG_GC
void printObjects(){
    Obj* curr = vm.objects;
    printf("Objects: ");
    if (curr == NULL) printf("None.\n");
    else printf("\n");
    while (curr != NULL){
        printf("       | ");
        printObject(OBJ_VAL(curr));
        printf("\n");
        curr = objNext(curr);
    }
}
#endif

void collectGarbage(){
    // triggers mark-and-sweep garbage collection
    // can be triggered during compile-time and runtime

    #ifdef DEBUG_LOG_GC
    printf("--gc begin--\n");
    printf("  initial: %zu\n", vm.bytesAllocated);
    size_t before = vm.bytesAllocated;
    #endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROWTH_FACTOR;

    #ifdef DEBUG_LOG_GC
    printf("-- gc end --\n");
    printf("   collected %zu bytes (from %zu to %zu), next collection at %zu\n", 
        before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
    #endif
}