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

    // initialize fields to default values, chain to vm.objects as head of linked list
    #ifdef OBJ_HEADER_COMPRESSION
    object->header = ((uint64_t)vm.objects << 8)  | (uint64_t)type;
    vm.objects = object;
    #else
    object->type = type;
    object->isMarked = false;
    object->isLocked = false;
    object->next = vm.objects;
    vm.objects = object;
    #endif

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
    setIsLocked(&string->obj, true);

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

ObjString* lambdaString(const char* prefix){
    const int length = (int)strlen(prefix) + 6;
    char* buffer = ALLOCATE(char, length + 1);
    snprintf(buffer, length + 1, "%s0x%04x", prefix, vm.counter++);
    buffer[length] = '\0';

    return takeString(buffer, length);
}
ObjString* printToString(const char* format, ...){
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char* buffer = ALLOCATE(char, length + 1);
    va_start(args, format);
    vsnprintf(buffer, length + 1, format, args);
    va_end(args);
    buffer[length] = '\0';
    
    return takeString(buffer, length);
}
ObjString* printFunctionToString(Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_NATIVE:    return printToString("<fn %s>", AS_NATIVE(value)->name->chars);
        case OBJ_FUNCTION:  return printToString("<fn %s>", AS_FUNCTION(value)->name->chars);
        case OBJ_CLOSURE:   return printToString("<fn %s>", AS_CLOSURE(value)->function->name->chars);
    }
    return takeString("", 0);
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
    function->fromTry = false;
    function->name = NULL;
    initChunk(&function->chunk);
    setIsLocked(&function->obj, true);

    return function;
}

// OBJFUNCTION METHODS
ObjNative* newNative(NativeFn function, int arity, ObjString* name){
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    native->name = name;
    setIsLocked(&native->obj, true);

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
    setIsLocked(&closure->obj, true);

    return closure;
}

// OBJCLASS METHODS
ObjClass* newClass(ObjString* name){
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    initTable(&klass->statics);
    setIsLocked(&klass->obj, true);

    return klass;
}

// OBJINSTANCE METHODS
ObjInstance* newInstance(ObjClass* klass){
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

// OBJBOUNDMETHOD METHODS
ObjBoundMethod* newBoundMethod(Value receiver, Obj* method){
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    setIsLocked(&bound->obj, true);

    return bound;
}

// EXCEPTION METHODS
ObjException* newException(Value payload){
    ObjException* exception = ALLOCATE_OBJ(ObjException, OBJ_EXCEPTION);
    exception->payload = payload;
    setIsLocked(&exception->obj, true);

    return exception;
}

// ARRAY-RELATED METHODS
ObjArray* newArray(){
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
    initValueArray(&array->data);
    return array;
}
ObjArraySlice* newSlice(Value start, Value end, Value step){
    ObjArraySlice* slice = ALLOCATE_OBJ(ObjArraySlice, OBJ_ARRAY_SLICE);
    slice->start = start;
    slice->end = end;
    slice->step = step;
    setIsLocked(&slice->obj, true);

    return slice;
}

// HASHMAP-RELATED METHODS
ObjHashmap* newHashmap(){
    ObjHashmap* hashmap = ALLOCATE_OBJ(ObjHashmap, OBJ_HASHMAP);
    initTable(&hashmap->data);
    return hashmap;
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
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars); break;
        case OBJ_INSTANCE:
            printf("<%s instance>", AS_INSTANCE(value)->klass->name->chars); break;
        case OBJ_BOUND_METHOD:
            printObject(OBJ_VAL(AS_BOUND_METHOD(value)->method)); break;

        case OBJ_EXCEPTION: {
            printf("Exception: ");
            printValue(AS_EXCEPTION(value)->payload);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = AS_ARRAY(value);
            int count = array->data.count;
            printf("[");
            for (int i = 0 ; i < count; i++){
                printValue(array->data.values[i]);
                if (i + 1 == count) break;    // Last item
                printf(", ");
            }
            printf("]");
            break;
        }
        case OBJ_ARRAY_SLICE: {
            ObjArraySlice* slice = AS_ARRAY_SLICE(value);
            printf("Slice: ");
            printValue(slice->start);
            printf(", ");
            printValue(slice->end);
            printf(", ");
            printValue(slice->step);
        }
        case OBJ_HASHMAP: {
            ObjHashmap* hashmap = AS_HASHMAP(value);
            printf("{ ");
            bool firstItem = true;
            for (int i = 0; i < hashmap->data.capacity; i++){
                if (hashmap->data.entries[i].key == EMPTY_VAL()) continue;
                if (firstItem) firstItem = false;
                else printf(", ");
                printValue(hashmap->data.entries[i].key);
                printf(":");
                printValue(hashmap->data.entries[i].value);
            }
            printf(" }");
        }
        default: break;    // Unreachable.
    }
}