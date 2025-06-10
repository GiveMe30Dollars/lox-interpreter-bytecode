#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// forward declaration of object.h related structs
// is defined in object.h
typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;


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
#define EMPTY_VAL(value)   ((Value){VAL_EMPTY, {.number = 0}})
// (The explicit cast suppresses some compilation warnings)

// TYPE CHECK MACROS
#define IS_BOOL(value)     ((value).type == VAL_BOOL)
#define IS_NIL(value)      ((value).type == VAL_NIL)
#define IS_NUMBER(value)   ((value).type == VAL_NUMBER)
#define IS_OBJ(value)      ((value).type == VAL_OBJ)
#define IS_EMPTY(value)      ((value).type == VAL_EMPTY)

// CAST MACROS
// (does not assert type!)
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_NUMBER(value)   ((value).as.number)
#define AS_OBJ(value)      ((value).as.obj)

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