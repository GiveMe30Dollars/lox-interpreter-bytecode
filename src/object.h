#ifndef clox_object_h
#define clox_object_h

#include <stdarg.h>

#include "common.h"
#include "chunk.h"
#include "hashtable.h"
#include "value.h"

#define OBJ_TYPE(value) (objType(AS_OBJ(value)))

// Relating to Obj and its derived structs
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,    // <-- End of vanilla
    OBJ_EXCEPTION,
    OBJ_ARRAY,
    OBJ_ARRAY_SLICE,
    OBJ_HASHMAP
} ObjType;

#ifdef OBJ_HEADER_COMPRESSION

// Bit representation:
// ...L...M NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN EEEEEEEE
// M: isMarked
// L: isLocked
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
static inline bool isLocked(Obj* object){
    return (bool)((object->header >> 60) & 0x01);
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
static inline void setIsLocked(Obj* object, bool isMarked){
    object->header = (object->header & 0x00ffffffffffffff) | ((uint64_t)isMarked << 60);
}
static inline void setObjNext(Obj* object, Obj* next){
    object->header = (object->header & 0xff000000000000ff) | ((uint64_t)next << 8);
}

#else

struct Obj {
    ObjType type;
    bool isMarked;
    bool isLocked;
    struct Obj* next;
};

static inline ObjType objType(Obj* object){
    return object->type;
}
static inline bool isMarked(Obj* object){
    return object->isMarked;
}
static inline bool isLocked(Obj* object){
    return object->isLocked;
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
static inline void setIsLocked(Obj* object, bool isLocked){
    object->isLocked = isLocked;
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
ObjString* lambdaString(const char* prefix);
ObjString* printToString(const char* format, ...);
ObjString* printFunctionToString(Value value);

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
    bool fromTry;
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


// ObjClass encapsulates a user-defined Lox class
typedef struct {
    Obj obj;
    ObjString* name;
    HashTable methods;
    HashTable statics;
} ObjClass;
ObjClass* newClass(ObjString* name);

// ObjInstances are created when an ObjClass is called
typedef struct {
    Obj obj;
    ObjClass* klass;
    HashTable fields;
    uint32_t hash;
} ObjInstance;
ObjInstance* newInstance(ObjClass* klass);

// ObjBoundMethods bind `this` and `super` to the instance where this method is accessed from
typedef struct {
    Obj obj;
    Value receiver;
    // can be ObjFunction or ObjClosure
    Obj* method;
} ObjBoundMethod;
ObjBoundMethod* newBoundMethod(Value receiver, Obj* method);


// Exceptions contain a payload
typedef struct {
    Obj obj;
    Value payload;
} ObjException;
ObjException* newException(Value payload);

// ObjArray exposes the ValueArray struct to a Lox end-user
typedef struct {
    Obj obj;
    ValueArray data;
    uint32_t hash;
} ObjArray;
ObjArray* newArray();

// ObjArraySlice is used for array slicing, and nothing else.
// Start inclusive, end exclusive
typedef struct {
    Obj obj;
    Value start;
    Value end;
    Value step;
} ObjArraySlice;
ObjArraySlice* newSlice(Value start, Value end, Value step);


// ObjHashmap exposes the HashTable struct to a Lox end-user
typedef struct {
    Obj obj;
    HashTable data;
    uint32_t hash;
} ObjHashmap;
ObjHashmap* newHashmap();


// OBJECT GENERAL FUNCTIONS
void printFunction(ObjFunction* function);
void printObject(Value value);

// TYPE CHECK MACROS / INLINE FUNCTIONS
#define IS_STRING(value)       (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value)     (isObjType(value, OBJ_FUNCTION))
#define IS_NATIVE(value)       (isObjType(value, OBJ_NATIVE))
#define IS_CLOSURE(value)      (isObjType(value, OBJ_CLOSURE))
#define IS_CLASS(value)        (isObjType(value, OBJ_CLASS))
#define IS_INSTANCE(value)     (isObjType(value, OBJ_INSTANCE))
#define IS_BOUND_METHOD(value) (isObjType(value, OBJ_BOUND_METHOD))

#define IS_EXCEPTION(value)    (isObjType(value, OBJ_EXCEPTION))
#define IS_ARRAY(value)        (isObjType(value, OBJ_ARRAY))
#define IS_ARRAY_SLICE(value)  (isObjType(value, OBJ_ARRAY_SLICE))
#define IS_HASHMAP(value)      (isObjType(value, OBJ_HASHMAP))

static inline bool isObjType(Value value, ObjType type){
    return IS_OBJ(value) && (objType(AS_OBJ(value)) == type);
}

// CAST MACROS
// (does not assert type!)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       ((ObjNative*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

#define AS_EXCEPTION(value)    ((ObjException*)AS_OBJ(value))
#define AS_ARRAY(value)        ((ObjArray*)AS_OBJ(value))
#define AS_ARRAY_SLICE(value)  ((ObjArraySlice*)AS_OBJ(value))
#define AS_HASHMAP(value)      ((ObjHashmap*)AS_OBJ(value))

#endif