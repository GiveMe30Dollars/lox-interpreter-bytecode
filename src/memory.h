#ifndef clox_memory_h
#define clox_memory_h

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, oldCount, newCount) \
    (type*)reallocate(ptr, oldCount * sizeof(type), newCount * sizeof(type))

#define FREE_ARRAY(type, ptr, oldCount) \
    (type*)reallocate(ptr, oldCount * sizeof(type), 0)

void* reallocate(void* ptr, size_t oldSize, size_t newSize);

#endif