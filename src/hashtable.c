#include "hashtable.h"

uint32_t hashBytes(const uint8_t* key, size_t numOfBytes){
    uint32_t hash = 2166136261u;
    for (int i = 0; i < numOfBytes; i++){
        hash ^= *(key + i);
        hash *= 16777619;
    }
    return hash;
}