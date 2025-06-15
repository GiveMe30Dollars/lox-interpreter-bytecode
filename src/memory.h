#ifndef clox_memory_h
#define clox_memory_h

#include "value.h"
#include "object.h"

// MEMORY ALLOCATION MACROS
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, (count) * sizeof(type))

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, oldCount, newCount) \
    (type*)reallocate(ptr, oldCount * sizeof(type), (newCount) * sizeof(type))

#define FREE(type, ptr) \
    (type*)reallocate(ptr, sizeof(type), 0)
    
#define FREE_ARRAY(type, ptr, oldCount) \
    (type*)reallocate(ptr, oldCount * sizeof(type), 0)

void* reallocate(void* ptr, size_t oldSize, size_t newSize);
void freeObjects();

void markObject(Obj* obj);
void markValue(Value value);
void collectGarbage();

#endif