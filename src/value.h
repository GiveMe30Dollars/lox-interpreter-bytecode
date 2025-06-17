#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

// forward declaration of Obj and ObjString
// is defined in object.h
typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef VALUE_NAN_BOXING


typedef uint64_t Value;

#define QNAN      ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT  ((uint64_t)0x8000000000000000)
#define TAG_EMPTY 0
#define TAG_NIL   1
#define TAG_FALSE 2
#define TAG_TRUE  3

static inline Value numToValue(double num){
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}
static inline double valueToNum(Value value){
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}


// VALUE CONSTRUCTOR MACROS

#define EMPTY_VAL()        ((Value)(uint64_t)(QNAN | TAG_EMPTY))
#define NIL_VAL()          ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL()        ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL()         ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b)        ((b) ? TRUE_VAL() : FALSE_VAL())
#define NUMBER_VAL(num)    (numToValue(num))
#define OBJ_VAL(obj)       (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
// (The explicit cast suppresses some compilation warnings)

// TYPE CHECK MACROS
#define IS_EMPTY(value)    ((value) == EMPTY_VAL())
#define IS_NIL(value)      ((value) == NIL_VAL())
#define IS_BOOL(value)     (((value) | 1) == TRUE_VAL())
#define IS_NUMBER(value)   (((value) & QNAN) != QNAN)
#define IS_OBJ(value)      (((value) & (SIGN_BIT | QNAN)) == (SIGN_BIT | QNAN))

// CAST MACROS
// (does not assert type!)
#define AS_BOOL(value)     ((value) == TRUE_VAL())
#define AS_NUMBER(value)   (valueToNum(value))
#define AS_OBJ(value)      ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#else

// Representation of Lox values:
// literals (boolean, nil, number) are stored directly
// objects are stored as pointer to heap-allocated Obj

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,      // stores Obj* pointer, which can be cast to its object type
    VAL_EMPTY,    // for internal use in hash table only. never exposed to user.
} ValueType;

typedef struct {
    ValueType type;
    union{
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// VALUE CONSTRUCTOR MACROS
#define BOOL_VAL(value)    ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL()          ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)  ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(value)     ((Value){VAL_OBJ, {.obj = (Obj*)value}})
#define EMPTY_VAL()        ((Value){VAL_EMPTY, {.number = 0}})
// (The explicit cast suppresses some compilation warnings)

// TYPE CHECK MACROS
#define IS_BOOL(value)     ((value).type == VAL_BOOL)
#define IS_NIL(value)      ((value).type == VAL_NIL)
#define IS_NUMBER(value)   ((value).type == VAL_NUMBER)
#define IS_OBJ(value)      ((value).type == VAL_OBJ)
#define IS_EMPTY(value)    ((value).type == VAL_EMPTY)

// CAST MACROS
// (does not assert type!)
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_NUMBER(value)   ((value).as.number)
#define AS_OBJ(value)      ((value).as.obj)

#endif

bool valuesEqual(Value a, Value b);


// Dynamically-resizing value array:
// this is used for chunk.constants array

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

void printValue(Value value);

void initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);

#endif