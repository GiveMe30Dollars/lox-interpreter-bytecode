#include <string.h>
#include <stdlib.h>

#include "hashtable.h"
#include "object.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

uint32_t hashBytes(const uint8_t* key, size_t numOfBytes){
    uint32_t hash = 2166136261u;
    for (int i = 0; i < numOfBytes; i++){
        hash ^= *(key + i);
        hash *= 16777619;
    }
    return hash;
}


void initTable(HashTable* table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}
void freeTable(HashTable* table){
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}


// HELPRE FUNCTIONS FOR HASH TABLE
static uint32_t getHash(Value value){
    switch(value.type){
        case VAL_NIL:    return 5;
        case VAL_BOOL:   return AS_BOOL(value) ? 3 : 7;
        case VAL_NUMBER:
            return HASH_NUMBER(value);
        case VAL_OBJ: {
            switch (OBJ_TYPE(value)){
                case OBJ_STRING:
                    return AS_STRING(value)->hash;
                case OBJ_FUNCTION:
                case OBJ_NATIVE:
                case OBJ_CLOSURE:
                case OBJ_CLASS:
                case OBJ_INSTANCE:
                case OBJ_BOUND_METHOD:
                {
                    union BitCast
                    {
                        Obj* pointer;
                        uint32_t ints[2];
                    };
                    union BitCast bitcast;
                    bitcast.pointer = AS_OBJ(value);
                    return bitcast.ints[0] + bitcast.ints[1];
                }
            }
        }
    }
    return 0;    // Unreachable.
}
static Entry* findEntry(Entry* entries, int capacity, Value key){
    // helper function for tableGet and tableSet
    // returns either a pointer to the found entry (search succeeded)
    // tombstone or an empty bucket, whichever is found first (search failed)
    // the probing is only terminated at an empty bucket for open addressing.

    uint32_t index = getHash(key) & (capacity - 1);
    Entry* tombstone = NULL;
    for (;;){
        // Due to hash tables never being completely full, not endless
        Entry* entry = &entries[index];
        if (IS_EMPTY(entry->key)){
            // empty bucket, return
            if (IS_NIL(entry->value)) return tombstone ? tombstone : entry;
            // tombstone, continue
            if (tombstone == NULL) tombstone = entry;
        } 
        else if (valuesEqual(entry->key, key)) {
            return entry;
        }
        
        // continue linear probing
        index = (index + 1) & (capacity - 1);
    }
}

static void adjustCapacity(HashTable* table, int capacity){
    // Reallocates hash table to specified capacity
    // Recalculation of count required because tombstones are not retained

    Entry* entries = ALLOCATE(Entry, capacity);
    table->count = 0;

    // Initialize new entries block, copy over existing non-tombstone values
    for (int i = 0; i < capacity; i++){
        entries[i].key = EMPTY_VAL();
        entries[i].value = NIL_VAL();
    }
    for (int i = 0; i < table->capacity; i++){
        Entry* target = &table->entries[i];
        if (IS_EMPTY(target->key)) continue;
        Entry* dest = findEntry(entries, capacity, target->key);
        dest->key = target->key;
        dest->value = target->value;
        table->count++;
    }

    // Free old entries, reassign pointers to new entries
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

/*
Empty     : {EMPTY, NIL}
Tombstone : {EMPTY, OTHER} (usually BOOL(true))
*/
bool tableSet(HashTable* table, Value key, Value value){
    // sets the entry with specified key in the hash table to value
    // returns whether the entry set is new

    // if at capacity, adjust table capacity
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD){
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = IS_EMPTY(entry->key) && !IS_BOOL(entry->value);
    if (isNewKey) table->count++;

    // empty, tombstone, or previous entry of that key are all valid set targets
    entry->key = key;
    entry->value = value;

    return isNewKey;
}
bool tableGet(HashTable* table, Value key, Value* output){
    // gets the entry with specified key, writes its value to location pointed by output
    // returns whether the entry exists and get operation succeeded

    if (table->capacity == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity, key);

    if (IS_EMPTY(entry->key)) return false;

    *output = entry->value;
    return true;
}
bool tableDelete(HashTable* table, Value key){
    // replaces the entry with specified key with a tombstone
    // returns whether the entry existed and the delete succeeded

    if (table->capacity == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) return false;

    entry->key = EMPTY_VAL();
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(HashTable* from, HashTable* to){
    // copies all non-tombstone entries from a table to another
    for (int i = 0; i < from->capacity; i++){
        Entry* target = &from->entries[i];
        if (!IS_EMPTY(target->key))
            tableSet(to, target->key, target->value);
    }
}


ObjString* tableFindString(HashTable* table, const char* string, int length, uint32_t hash){
    // if the string is interned in table as key, return that pointer
    // else return NULL; the caller handles creating a new one
    // this ensures all ObjString*s can be compared by pointer value.

    // note: this is the only place where memcmp will be used for ObjString comparison

    if (table->capacity == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;){
        Entry* entry = &table->entries[index];
        if (IS_EMPTY(entry->key)){
            // empty bucket, return
            if (IS_NIL(entry->value)) return NULL;
            // tombstone, continue
        }
        else if (IS_STRING(entry->key) && AS_STRING(entry->key)->length == length
            && AS_STRING(entry->key)->hash == hash && (memcmp(string, AS_CSTRING(entry->key), length) == 0)){
            // string found.
            return AS_STRING(entry->key);
        }
        index = (index + 1) & (table->capacity - 1);
    } 
}


// MEMORY METHODS

void tableRemoveWhite(HashTable* table){
    for (int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if (!IS_EMPTY(entry->key) && IS_OBJ(entry->value) && !(isMarked(AS_OBJ(entry->value))) ){
            // storing reference to unreachable object. delete entry
            tableDelete(table, entry->key);
        }
    }
}
void markTable(HashTable* table){
    for (int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        markValue(entry->key);
        markValue(entry->value);
    }
}