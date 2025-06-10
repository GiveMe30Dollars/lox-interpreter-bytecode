#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// Relating to Obj and its derived structs
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE
} ObjType;

struct Obj{
    ObjType type;
    struct Obj* next;
};


// all derived classes of Obj include Obj as its first field
// this enables pointer upcasting (ObjString* -> Obj*)
// if type matches, can safely downcast


// strings (immutable in Lox) cahce their own hash
// this makes string lookups in hash tables faster
// computation of hash is O(N) for its length
struct ObjString{
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};
ObjString* copyString(const char* start, int length);
ObjString* takeString(char* start, int length);


// ObjFunctions are created during compile time
typedef struct{
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;
ObjFunction* newFunction();

// ObjNatives are distinct from ObjFunctions
typedef Value (*NativeFn)(int argCount, Value* args);
typedef struct {
    Obj obj;
    int arity;
    NativeFn function;
} ObjNative;
ObjNative* newNative(NativeFn function);


// OBJECT GENERAL FUNCTIONS
void printFunction(ObjFunction* function);
void printObject(Value value);

// TYPE CHECK MACROS / INLINE FUNCTIONS
#define IS_STRING(value)   (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value) (isObjType(value, OBJ_FUNCTION))
#define IS_NATIVE(value) (isObjType(value, OBJ_NATIVE))

static inline bool isObjType(Value value, ObjType type){
    return IS_OBJ(value) && (AS_OBJ(value)->type == type);
}

// CAST MACROS
// (does not assert type!)
#define AS_STRING(value)   ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)  (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)

#endif