#ifndef clox_common_h
#define clox_common_h

// Contains all dependencies required by clox

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

// VM FLAGS (can enable/disable)

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE

// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

#define OBJ_HEADER_COMPRESSION
#define VALUE_NAN_BOXING

// #define IS_FALSEY_EXTENDED

#endif