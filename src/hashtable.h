#ifndef clox_hashtable_h
#define clox_hashtable_h

#include "common.h"
#include "value.h"

// HASHING ALGORITHM (FNV-1a)
uint32_t hashBytes(const uint8_t* key, size_t numOfBytes);
uint32_t getHash(Value value);

// ACCESS MACROS
#define HASH_CSTRING(string, length) (hashBytes((uint8_t*)(string), length * sizeof(char)))
#define HASH_NUMBER(value)           (hashEight((uint64_t)AS_NUMBER(value)))
#define HASH_POINTER(ptr)            (hashEight((uint64_t)(uintptr_t)ptr))


// HASH TABLE
typedef struct {
    Value key;
    Value value;
} Entry;
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} HashTable;

void initTable(HashTable* table);
void freeTable(HashTable* table);

bool tableSet(HashTable* table, Value key, Value value);
bool tableGet(HashTable* table, Value key, Value* output);
bool tableDelete(HashTable* table, Value key);

void tableAddAll(HashTable* from, HashTable* to);
ObjString* tableFindString(HashTable* table, const char* string, int length, uint32_t hash);

void tableRemoveWhite(HashTable* table);
void markTable(HashTable* table);

#endif