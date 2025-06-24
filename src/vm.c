#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "io.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "value.h"

// global variable
VM vm;


// VM HELPER FUNCTIONS
static void resetStack(){
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

void push(Value value){
    *vm.stackTop = value;
    vm.stackTop++;
}
Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}
static Value peek(int distance){
    return vm.stackTop[-1 - distance];
}

static inline ObjFunction* getFrameFunction(CallFrame* frame){
    if (objType(frame->function) == OBJ_FUNCTION){
        return (ObjFunction*)frame->function;
    } else {
        return ((ObjClosure*)frame->function)->function;
    }
}


static void runtimeError(const char* format, ...){
    // printf-style message to stderr
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // Stack trace
    for (int i = vm.frameCount - 1; i >= 0; i--){
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = getFrameFunction(frame);
        size_t instruction = frame->ip - function->chunk.code - 1;

        fprintf(stderr, "[line %d] in ", getLine(&function->chunk, instruction));
        if (function->name == NULL){
            fprintf(stderr, "<script>\n");
        } else {
            fprintf(stderr, "%s\n", function->name->chars);
        }
    }

    resetStack();
}

static bool throwValue(Value* payload){
    // returns true if stack recovery is successful
    // returns false for uncaught exceptions
    int newFrameCount = 0;
    for (int i = vm.frameCount - 1; i >= 0; i--){
        CallFrame* frame = &vm.frames[i];
        if (getFrameFunction(frame)->fromTry){
            newFrameCount = i;
            break;
        }
    }
    if (newFrameCount == 0){
        runtimeError("Uncaught %s", AS_CSTRING(stringPrimitiveNative(1, payload)) );
        return false;
    }

    // recover global variables
    vm.stackTop = vm.frames[newFrameCount].slots;
    vm.frameCount = newFrameCount;
    push(*payload);

    // skip over OP_POP and OP_JUMP following OP_TRY_CALL
    vm.frames[newFrameCount - 1].ip += 4;

    return true;
}
static bool runtimeException(const char* format, ...){
    // create a printf-style message to an ObjString, pushed onto the stack
    // then, throw it.
    va_list args;

    va_start(args, format);
    int bufferSize = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char* buffer = ALLOCATE(char, bufferSize + 1);
    va_start(args, format);
    vsnprintf(buffer, bufferSize + 1, format, args);
    buffer[bufferSize] = '\0';
    va_end(args);

    // push onto stack to prevent garbage collector headaches
    Value payload = OBJ_VAL(takeString(buffer, bufferSize));
    push(payload);
    Value exception = OBJ_VAL(newException(payload));
    push(exception);
    
    return throwValue(vm.stackTop - 1);
}

// forward declaration of callFunction for use in running stl.lox
static bool callFunction(ObjFunction* function, int  argCount);
static InterpreterResult run(bool isSTL);

// native function, method and static method parsing
static int defineNative(ImportNative native, bool isStaticMethod, const int i){
    // push onto stack to ensure they survive if GC triggered by reallocation of hash table
    // also handles nonstatic and static methods
    push(OBJ_VAL( copyString(native.name, (int)strlen(native.name)) ));
    push(OBJ_VAL( newNative(native.function, native.arity, AS_STRING(peek(0))) ));
    HashTable* target = vm.stackTop - vm.stack > 2 ? &AS_CLASS(peek(2))->methods : &vm.stl;
    tableSet(target, peek(1), peek(0));
    if (isStaticMethod) 
        tableSet(&AS_CLASS(peek(2))->statics, peek(1), peek(0));
    pop();
    pop();
    return i + 1;
}
static int defineSentinel(ImportSentinel sentinel, ImportInfo imports, const int i){
    // push onto stack to ensure they survive GC
    // also sets native static and nonstatic methods
    push(OBJ_VAL( copyString(sentinel.name, (int)strlen(sentinel.name)) ));
    push(OBJ_VAL( newClass(AS_STRING(peek(0))) ));
    for (int j = 1; j <= sentinel.numOfMethods; j++){
        ImportStruct method = imports.start[i + j];
        defineNative(method.as.native, (method.header == IMPORT_STATIC), j);
    }
    tableSet(&vm.stl, peek(1), peek(0));
    pop();
    pop();
    return i + sentinel.numOfMethods + 1;
}

void stl(){
    ImportInfo imports = buildSTL();
    for (int i = 0; i < imports.count; /* manual increment */ ){
        ImportStruct imp = imports.start[i];
        if (imp.header == IMPORT_NATIVE){
            i = defineNative(imp.as.native, false, i);
        } else {
            i = defineSentinel(imp.as.sentinel, imports, i);
        }
    }
    freeSTL(imports);
/*
    ObjFunction* stl = compile(readFile("src/stl.lox"), false);
    if (stl == NULL){
        fprintf(stderr, "STL failed to compile!");
        exit(74);
    }
    push(OBJ_VAL(stl));
    callFunction(stl, 0);
    if (run(true) != INTERPRETER_OK){
        fprintf(stderr, "STL failed to run!");
        exit(74);
    }
    else return;*/
}

// INITIALIZE/FREE VM
void initVM(){
    resetStack();
    vm.initString = NIL_VAL();

    // do this BEFORE anything, really
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initTable(&vm.stl);
    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.openUpvalues = NULL;
    vm.objects = NULL;
    vm.counter = 0;
    vm.initString = OBJ_VAL(copyString("init", 4));

    stl();
}
void freeVM(){
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NIL_VAL();
    freeObjects();
}

// RUNTIME HELPER FUNCTIONS

static bool isFalsey(Value value){
    // returns true for false and nil
    return (IS_NIL(value) || (IS_BOOL(value) && AS_BOOL(value) == false));
}

static void concatenateTwo(){
    // concatenates two strings on the stack
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}


static bool callNative(ObjNative* native, int argCount){
    // arity fail
    if (native->arity >= 0 && native->arity != argCount){
        return runtimeException("<fn %s> expected %d arguments but got %d.", native->name->chars, native->arity, argCount);
    }
    int callframeCount = vm.frameCount;
    Value result = (native->function)(argCount, vm.stackTop - argCount);
    if ( !IS_EMPTY(result) ){
        vm.stackTop -= (argCount + 1);
        push(result);
        return true;
    } else {
        if (vm.frameCount != callframeCount){
            // Native function passed execution to non-native function
            return true;
        }
        vm.stackTop -= argCount;
        return throwValue(vm.stackTop - 1);
    }
}
static bool call(Obj* callee, ObjFunction* function, int argCount){
    // attempts to write new call frame to VM

    // fail if arity not the same
    if (function->arity != argCount){
        return runtimeException("<fn %s> expected %d arguments but got %d.", function->name->chars, function->arity, argCount);
    }
    // stack overflow fail
    if (vm.frameCount == FRAMES_MAX){
        runtimeError("Stack overflow.");
        return false;
    }
    // success; create new call frame
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = (Obj*)callee;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}
static bool callFunction(ObjFunction* function, int argCount){
    return call((Obj*)function, function, argCount);
}
static bool callClosure(ObjClosure* closure, int argCount){
    return call((Obj*)closure, closure->function, argCount);
}
bool callValue(Value callee, int argCount){
    // returns false if call failed
    if (IS_OBJ(callee)){
        switch(OBJ_TYPE(callee)){
            case OBJ_NATIVE: 
                return callNative(AS_NATIVE(callee), argCount);
            case OBJ_FUNCTION:
                return callFunction(AS_FUNCTION(callee), argCount);
            case OBJ_CLOSURE:
                return callClosure(AS_CLOSURE(callee), argCount);
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[- argCount - 1] = OBJ_VAL(newInstance(klass));
                // if there is an initializer, call that too.
                Value initializer;
                if (tableGet(&klass->methods, vm.initString, &initializer)){
                    return callValue(initializer, argCount);
                } else if (argCount != 0) {
                    return runtimeException("<class %s> initializer expected 0 arguments but got %d.", klass->name->chars, argCount);
                }
                return true;
            }
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                // let's "this" refer to object method is bound to
                vm.stackTop[- argCount - 1] = bound->receiver;
                return callValue(OBJ_VAL(bound->method), argCount);
            }
            default: ;    // Fallthrough to failure case
        }
    }
    return runtimeException("Object is not callable.");
}


static ObjUpvalue* captureUpvalue(Value* local){
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    // iterate through the upvalues list while the stack location is less than the requested upvalue location
    while (upvalue != NULL && upvalue->location > local){
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }
    if (upvalue != NULL && upvalue->location == local){
        // found existing open upvalue, reuse.
        return upvalue;
    }

    // we either completely gone through the upvalues list, or the upvalue for this position does not exist.
    // create new upvalue and chain it into the sorted linked list
    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL){
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    // return created upvalue
    return createdUpvalue;
}
static void closeUpvalues(Value* last){
    // closes all upvalues that are located above 'last' stack slot
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last){
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}


static void defineMethod(Value name){
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}
static void defineStaticMethod(Value name){
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    tableSet(&klass->statics, name, method);
    pop();
}
static bool bindMethod(ObjClass* klass, Value name){
    // returns true if method is found and bounded
    // resultant ObjBoundMethod is pushed to the stack
    Value method;
    if (!tableGet(&klass->methods, name, &method)){
        return runtimeException("Undefined property '%s'.", AS_CSTRING(name));
    }
    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_OBJ(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}


static bool invokeFromClass(ObjClass* klass, Value name, int argCount){
    Value method;
    if (!tableGet(&klass->methods, name, &method)){
        return runtimeException("Undefined property '%s'.", AS_CSTRING(name));
    }
    return callValue(method, argCount);
}
bool invoke(Value name, int argCount){
    Value receiver = peek(argCount);
    if (IS_INSTANCE(receiver)){
        ObjInstance* instance = AS_INSTANCE(receiver);
        Value value;
        if (tableGet(&instance->fields, name, &value)){
            // treat as regular call by replacing 'this' with field
            vm.stackTop[- argCount - 1] = value;
            return callValue(value, argCount);
        }
        return invokeFromClass(instance->klass, name, argCount);
    } else if (IS_CLASS(receiver)){
        // static method
        ObjClass* klass = AS_CLASS(receiver);
        Value value;
        if (tableGet(&klass->statics, name, &value)) {
            // Treat as regular call
            vm.stackTop[- argCount - 1] = value;
            return callValue(value, argCount);
        } else {
            // no static method found
            return runtimeException("No static method of name '%s'.", AS_CSTRING(name));
        }
    } else {
        // Find sentinel
        Value sentinel = typeNative(1, &receiver);
        if (!IS_EMPTY(sentinel)){
            // sentinel class found.
            return invokeFromClass(AS_CLASS(sentinel), name, argCount);
        }
        // Nothing.
        return runtimeException("Object does not have methods.");
    }
}


static InterpreterResult run(bool isSTL){

    // Get current call frame
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    // Store instruction pointer as native CPU register
    register uint8_t* ip = frame->ip;

    // Preprocessor macros for reading bytes
    #define READ_BYTE()     (*ip++)
    #define READ_SHORT()    (ip += 2, (uint16_t)ip[-2] << 8 | ip[-1])
    #define READ_CONSTANT() (getFrameFunction(frame)->chunk.constants.values[READ_BYTE()])
    #define READ_STRING()   (AS_STRING(READ_CONSTANT()))

    #define SAVE_IP()       (frame->ip = ip)
    #define LOAD_IP() \
        do { \
            frame = &vm.frames[vm.frameCount - 1]; \
            ip = frame->ip; \
        } while (false)
    #define THROW(runtimeCall) \
        do { \
            SAVE_IP(); \
            if (runtimeCall == false) return INTERPRETER_RUNTIME_ERROR; \
            else{ \
                LOAD_IP(); \
                break; \
            } \
        } while (false)
    
    #define BINARY_OP(valueType, op, alt)\
        do{ \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))){ \
                Value methodString = OBJ_VAL(copyString(alt, (int)strlen(alt))); \
                SAVE_IP(); \
                if (invoke(methodString, 1)){ \
                    LOAD_IP(); \
                    break; \
                } else return INTERPRETER_RUNTIME_ERROR;\
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(valueType(a op b)); \
        } while (false)


    // ip is the incrementer, and is changed internally
    for (;;){
        
        #ifdef DEBUG_TRACE_EXECUTION
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++){
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&getFrameFunction(frame)->chunk, 
            (int)(ip - getFrameFunction(frame)->chunk.code));
        #endif

        // excecute instruction
        uint8_t instruction;
        switch (instruction = READ_BYTE()){
            case OP_CONSTANT:{
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_NIL:   push(NIL_VAL()); break;
            case OP_TRUE:  push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_DUPLICATE: {
                push(peek(READ_BYTE()));
                break;
            }
            case OP_POP:   pop(); break;
            case OP_POPN: {
                vm.stackTop -= READ_BYTE();
                break;
            }

            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT();
                HashTable* table = isSTL ? &vm.stl : &vm.globals;
                tableSet(table, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                Value name = READ_CONSTANT();
                Value value;
                if (!tableGet(&vm.globals, name, &value) && !tableGet(&vm.stl, name, &value)){
                    THROW(runtimeException("Undefined variable '%s'", AS_CSTRING(name)));
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                HashTable* table = isSTL ? &vm.stl : &vm.globals;
                if (tableSet(table, name, peek(0))){
                    // isNewKey returned true. cannot set undeclared global variable.
                    tableDelete(table, name);
                    THROW(runtimeException("Undefined variable '%s'.", AS_CSTRING(name)));
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*((ObjClosure*)frame->function)->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *((ObjClosure*)frame->function)->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_STL: {
                Value name = READ_CONSTANT();
                Value value;
                if (!tableGet(&vm.stl, name, &value)){
                    SAVE_IP();
                    runtimeError("Undefined STL identifier '%s'", AS_CSTRING(name));
                    return INTERPRETER_RUNTIME_ERROR;
                }
                push(value);
                break;
            }

            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >, "less"); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <, "greater"); break;

            case OP_ADD:      BINARY_OP(NUMBER_VAL, +, "add"); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -, "subtract"); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *, "multiply"); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /, "divide"); break;

            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))){
                    SAVE_IP();
                    runtimeError("Operand must be a number.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                push( NUMBER_VAL( -(AS_NUMBER(pop())) ) );
                break;
            
            case OP_PRINT: {
                Value toStringName = OBJ_VAL(copyString("toString", 8));
                push(toStringName);
                Value toStringFunction = hasMethodNative(2, vm.stackTop - 2);
                pop();
                if (!IS_NIL(toStringFunction)){
                    // call toString in separate callframe, then return to this instruction
                    ip--;
                    THROW(invoke(toStringName, 0));
                    break;
                }
                printValue(pop());
                printf("\n");
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t jump = READ_SHORT();
                if (isFalsey(peek(0))) ip += jump;
                break;
            }
            case OP_JUMP: {
                uint16_t jump = READ_SHORT();
                ip += jump;
                break;
            }
            case OP_LOOP: {
                uint16_t jump = READ_SHORT();
                ip -= jump;
                break;
            }
            
            case OP_CALL: {
                int argCount = READ_BYTE();
                SAVE_IP();
                if (!callValue(peek(argCount), argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // new call frame on stack, new register pointer
                LOAD_IP();
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++){
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal){
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = ((ObjClosure*)frame->function)->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OP_RETURN:{
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0){
                    pop();
                    return INTERPRETER_OK;
                }
                // discard call frame, restore universal variables (stack top)
                // then push result
                vm.stackTop = frame->slots;
                push(result);
                // update frame and ip
                LOAD_IP();
                break;
            }

            case OP_TRY_CALL: {
                SAVE_IP();
                // No need to check for type safety
                callValue(peek(0), 0);
                LOAD_IP();
                break;
            }
            case OP_THROW: {
                // store ip from register to callframe (not that it matters for anything other than error reporting)
                SAVE_IP();
                if (throwValue(vm.stackTop - 1) == false)
                    return INTERPRETER_RUNTIME_ERROR;
                // new call frame on stack, new register pointer
                LOAD_IP();
                break;
            }

            case OP_CLASS: {
                ObjString* name = READ_STRING();
                Value klass;
                // if isSTL and class has sentinel counterpart, open that class
                if (isSTL && tableGet(&vm.stl, OBJ_VAL(name), &klass)){
                    push(klass);
                    break;
                }
                push(OBJ_VAL(newClass(name)));
                break;
            }
            case OP_GET_PROPERTY: {
                ObjClass* klass = NULL;
                Value name = READ_CONSTANT();
                SAVE_IP();
                if (IS_INSTANCE(peek(0))){
                    // if property found, use that
                    // else look for class methods
                    ObjInstance* instance = AS_INSTANCE(peek(0));
                    Value value;
                    if (tableGet(&instance->fields, name, &value)){
                        pop();    // Pop instance
                        push(value);
                        break;
                    }
                    klass = instance->klass;
                } else if (IS_CLASS(peek(0))){
                    // look for static method
                    // no binding required
                    Value klass = peek(0);
                    Value value;
                    if (tableGet(&AS_CLASS(klass)->statics, name, &value)){
                        pop();
                        push(value);
                        break;
                    } else {
                        THROW(runtimeException("No static method of name '%s'.", AS_CSTRING(name)));
                    }
                } else {
                    // look for sentinel class methods
                    Value value = peek(0);
                    Value sentinel = typeNative(1, &value);
                    if (IS_EMPTY(sentinel)){
                        THROW(runtimeException("This object does not have properties."));
                    } else {
                        klass = AS_CLASS(sentinel);
                    }
                }
                if (!bindMethod(klass, name)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // update frame and ip
                LOAD_IP();
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))){
                    THROW(runtimeException("Only instances have fields."));
                }
                ObjInstance* instance = AS_INSTANCE(peek(1));
                Value name = READ_CONSTANT();
                tableSet(&instance->fields, name, peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_METHOD: {
                defineMethod(READ_CONSTANT());
                break;
            }
            case OP_STATIC_METHOD: {
                defineStaticMethod(READ_CONSTANT());
                break;
            }
            case OP_INVOKE: {
                Value method = READ_CONSTANT();
                int argCount = READ_BYTE();
                SAVE_IP();
                if (!invoke(method, argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // update frame and ip
                LOAD_IP();
                break;
            }
            case OP_INHERIT: {
                Value predecessor = peek(1);
                if (IS_CLASS(predecessor)){
                    ObjClass* subclass = AS_CLASS(peek(0));
                    tableAddAll(&AS_CLASS(predecessor)->methods, &subclass->methods);
                    tableAddAll(&AS_CLASS(predecessor)->statics, &subclass->statics);
                    // Pop subclass. Superclass remains as local variable.
                    pop();
                    break;
                } else {
                    THROW(runtimeException("Superclass must be a class or an array of classes."));
                }
            }
            case OP_INHERIT_MULTIPLE: {
                ObjClass* subclass = AS_CLASS(peek(0));
                ObjArray* superclassArray = AS_ARRAY(peek(1));
                for (int i = superclassArray->data.count - 1; i >= 0; i--){
                    Value superclass = superclassArray->data.values[i];
                    if (!IS_CLASS(superclass)){
                        THROW(runtimeException("Element must be a class for multiple inheritance."));
                    }
                    tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                    tableAddAll(&AS_CLASS(superclass)->statics, &subclass->statics);
                }
                // Pop subclass. Superclass array remains as local variable.
                pop();
                break;
            }
            case OP_GET_SUPER: {
                Value name = READ_CONSTANT();
                ObjClass* superclass = AS_CLASS(pop());
                if (!bindMethod(superclass, name)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUPER_INVOKE: {
                Value method = READ_CONSTANT();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop());
                SAVE_IP();
                if (!invokeFromClass(superclass, method, argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                LOAD_IP();
                break;
            }
        }    // end switch
    }        // end loop

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef SAVE_IP
    #undef LOAD_IP
    #undef THROW
    #undef BINARY_OP
}
InterpreterResult interpret(const char* source, bool evalExpr){
    ObjFunction* topLevelCode = compile(source, evalExpr);
    if (topLevelCode == NULL)
        return INTERPRETER_COMPILE_ERROR;

    // push top-level call frame onto stack
    push(OBJ_VAL(topLevelCode));
    callFunction(topLevelCode, 0);

    return run(false);
}