#ifndef clox_value_h
#define clox_value_h

typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

void printValue(Value value);

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

#endif