#ifndef clox_hashtable_h
#define clox_hashtable_h

#include "common.h"
#include "value.h"
#include "object.h"

// HASHING ALGORITHM (FNV-1a)
uint32_t hashBytes(const uint8_t* key, size_t numOfBytes);

// ACCESS MACROS
#define HASH_STRING(value) (hashBytes((uint8_t*)AS_CSTRING(value), AS_STRING(value)->length))
#define HASH_DOUBLE(value) (hashBytes((uint8_t*)(&(AS_NUMBER(value))), sizeof(double)))

#endif