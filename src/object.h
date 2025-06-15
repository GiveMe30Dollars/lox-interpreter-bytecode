#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (objType(AS_OBJ(value)))

// Relating to Obj and its derived structs
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE
} ObjType;

#ifdef OBJ_HEADER_COMPRESSION

// Bit representation:
// .......M NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN EEEEEEEE
// M: isMarked
// N: next pointer
// E: ObjType enum

struct Obj {
    uint64_t header;
};

static inline ObjType objType(Obj* object){
    return (ObjType)(object->header & 0xff);
}
static inline bool isMarked(Obj* object){
    return (bool)((object->header >> 56) & 0x01);
}
static inline Obj* objNext(Obj* object){
    return (Obj*)((object->header & 0x00ffffffffffff00) >> 8);
}

static inline void setObjType(Obj* object, ObjType type){
    object->header = (object->header & 0xffffffffffffff00) | (uint64_t)type;
}
static inline void setIsMarked(Obj* object, bool isMarked){
    object->header = (object->header & 0x00ffffffffffffff) | ((uint64_t)isMarked << 56);
}
static inline void setObjNext(Obj* object, Obj* next){
    object->header = (object->header & 0xff000000000000ff) | ((uint64_t)next << 8);
}

#else

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

static inline ObjType objType(Obj* object){
    return object->type;
}
static inline bool isMarked(Obj* object){
    return object->isMarked;
}
static inline Obj* objNext(Obj* object){
    return object->next;
}

static inline void setObjType(Obj* object, ObjType type){
    object->type = type;
}
static inline void setIsMarked(Obj* object, bool isMarked){
    object->isMarked = isMarked;
}
static inline void setObjNext(Obj* object, Obj* next){
    object->next = next;
}

#endif

// all derived classes of Obj include Obj as its first field
// this enables pointer upcasting (ObjString* -> Obj*)
// if type matches, can safely downcast


// strings (immutable in Lox) cahce their own hash
// this makes string lookups in hash tables faster
// computation of hash is O(N) for its length
struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};
ObjString* copyString(const char* start, int length);
ObjString* takeString(char* start, int length);

ObjString* lambdaString();

// ObjUpvalues are created when open upvalues are found, and store closed upvalues
typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;
ObjUpvalue* newUpvalue(Value* location);


// ObjFunctions are created during compile time
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
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
    ObjString* name;
} ObjNative;
ObjNative* newNative(NativeFn function, int arity, ObjString* name);

// ObjClosures store upvalues for ObjFunctions
typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;
ObjClosure* newClosure(ObjFunction* function);



// OBJECT GENERAL FUNCTIONS
void printFunction(ObjFunction* function);
void printObject(Value value);

// TYPE CHECK MACROS / INLINE FUNCTIONS
#define IS_STRING(value)     (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value)   (isObjType(value, OBJ_FUNCTION))
#define IS_NATIVE(value)     (isObjType(value, OBJ_NATIVE))
#define IS_CLOSURE(value)    (isObjType(value, OBJ_CLOSURE))

static inline bool isObjType(Value value, ObjType type){
    return IS_OBJ(value) && (objType(AS_OBJ(value)) == type);
}

// CAST MACROS
// (does not assert type!)
#define AS_STRING(value)     ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)    (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)   ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)     ((ObjNative*)AS_OBJ(value))
#define AS_CLOSURE(value)    ((ObjClosure*)AS_OBJ(value))

#endif