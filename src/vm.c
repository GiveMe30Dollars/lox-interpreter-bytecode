#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "value.h"

// global variable
VM vm;

// native function/method parsing
static void defineNative(const char* name, NativeFn function, int arity){
    // push onto stack to ensure they survive if GC triggered by reallocation of hash table
    push(OBJ_VAL( copyString(name, (int)strlen(name)) ));
    push(OBJ_VAL( newNative(function, arity, AS_STRING(vm.stack[0])) ));
    tableSet(&vm.stl, vm.stack[0], vm.stack[1]);
    pop();
    pop();
}

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

void stl(){
    ImportInfo imports = buildSTL();
    for (int i = 0; i < imports.count; /* manual increment */ ){
        ImportStruct imp = imports.start[i];
        if (imp.header == IMPORT_NATIVE){
            ImportNative native = imp.as.native;
            defineNative(native.name, native.function, native.arity);
            i++;
        } else {
            ImportSentinel sentinel = imp.as.sentinel;
            push(OBJ_VAL( copyString(sentinel.name, (int)strlen(sentinel.name)) ));
            push(OBJ_VAL( newClass(AS_STRING(peek(0))) ));
            i++;
            for (int j = 0; j < sentinel.numOfMethods; j++){
                ImportNative method = imports.start[i].as.native;
                push(OBJ_VAL( copyString(method.name, (int)strlen(method.name)) ));
                push(OBJ_VAL( newNative(method.function, method.arity, AS_STRING(peek(0))) ));
                tableSet(&(AS_CLASS(peek(2))->methods), peek(1), peek(0));
                pop();
                pop();
                i++;
            }
            tableSet(&vm.stl, vm.stack[0], vm.stack[1]);
            pop();
            pop();
        }
    }
    freeSTL(imports);
}

// INITIALIZE/FREE VM
void initVM(){
    resetStack();

    initTable(&vm.stl);
    initTable(&vm.globals);
    initTable(&vm.strings);

    // do this BEFORE initString
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    vm.openUpvalues = NULL;
    vm.objects = NULL;
    vm.hash = 0;
    vm.initString = NIL_VAL();
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
    #ifdef IS_FALSEY_EXTENDED
    // returns true for: false, nil, <empty>, 0, ""
    // (an enduser should not have access to <empty>)
    switch (value.type){
        case VAL_NIL:    return true;
        case VAL_BOOL:   return !AS_BOOL(value);
        case VAL_EMPTY:  return true;
        case VAL_NUMBER: return AS_NUMBER(value) == 0;
        case VAL_OBJ:    return AS_STRING(value)->length == 0;
        default:         return true;    // Unreachable.
    }
    #else
    // returns true for false and nil
    return (IS_NIL(value) || (IS_BOOL(value) && AS_BOOL(value) == false));
    #endif
}

static inline ObjFunction* getFrameFunction(CallFrame* frame){
    if (objType(frame->function) == OBJ_FUNCTION){
        return (ObjFunction*)frame->function;
    } else {
        return ((ObjClosure*)frame->function)->function;
    }
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


static bool callNative(ObjNative* native, int argCount){
    // arity fail
    if (native->arity >= 0 && native->arity != argCount){
        runtimeError("Expected %d arguments but got %d.", native->arity, argCount);
        return false;
    }
    Value result = (native->function)(argCount, vm.stackTop - argCount);
    if ( !IS_EMPTY(result) ){
        vm.stackTop -= (argCount + 1);
        push(result);
        return true;
    } else {
        vm.stackTop -= argCount;
        runtimeError("Uncaught exception: %s", AS_CSTRING(stringNative(1, (vm.stackTop - 1))) );
        return false;
    }
}
static bool call(Obj* callee, ObjFunction* function, int argCount){
    // attempts to write new call frame to VM

    // fail if arity not the same
    if (function->arity != argCount){
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
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
static bool callValue(Value callee, int argCount){
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
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
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
    runtimeError("Can only call functions and classes.");
    return false;
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
static bool bindMethod(ObjClass* klass, Value name){
    // returns true if method is found and bounded
    // resultant ObjBoundMethod is pushed to the stack
    Value method;
    if (!tableGet(&klass->methods, name, &method)){
        runtimeError("Undefined property '%s'.", AS_CSTRING(name));
        return false;
    }
    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_OBJ(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}


static bool invokeFromClass(ObjClass* klass, Value name, int argCount){
    Value method;
    if (!tableGet(&klass->methods, name, &method)){
        runtimeError("Undefined property '%s'", AS_CSTRING(name));
        return false;
    }
    return callValue(method, argCount);
}
static bool invoke(Value name, int argCount){
    Value receiver = peek(argCount);
    if (!IS_INSTANCE(receiver)){
        Value sentinel = typeNative(1, &receiver);
        if (!IS_EMPTY(sentinel)){
            // sentinel class found.
            return invokeFromClass(AS_CLASS(sentinel), name, argCount);
        }
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(receiver);
    Value value;
    if (tableGet(&instance->fields, name, &value)){
        // treat as regular call by replacing 'this' with field (function object?)
        vm.stackTop[- argCount - 1] = value;
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
}


static InterpreterResult run(){

    // Get current call frame
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    // Store instruction pointer as native CPU register
    register uint8_t* ip = frame->ip;

    // Preprocessor macros for reading bytes
    #define READ_BYTE() (*ip++)
    #define READ_SHORT() (ip += 2, (uint16_t)ip[-2] << 8 | ip[-1])
    #define READ_CONSTANT() (getFrameFunction(frame)->chunk.constants.values[READ_BYTE()])
    #define READ_STRING() (AS_STRING(READ_CONSTANT()))
    
    #define BINARY_OP(valueType, op)\
        do{ \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))){ \
                frame->ip = ip; \
                runtimeError("Operands must be numbers."); \
                return INTERPRETER_RUNTIME_ERROR; \
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
                push(peek(0));
                break;
            }
            case OP_POP:   pop(); break;
            case OP_POPN: {
                vm.stackTop -= READ_BYTE();
                break;
            }

            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                Value name = READ_CONSTANT();
                Value value;
                if (!tableGet(&vm.globals, name, &value) && !tableGet(&vm.stl, name, &value)){
                    frame->ip = ip;
                    runtimeError("Undefined variable '%s'", AS_CSTRING(name));
                    return INTERPRETER_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                if (tableSet(&vm.globals, name, peek(0))){
                    // isNewKey returned true. cannot set undeclared global variable.
                    frame->ip = ip;
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", AS_CSTRING(name));
                    return INTERPRETER_RUNTIME_ERROR;
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
                    frame->ip = ip;
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
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;

            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))){
                    concatenateTwo();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    frame->ip = ip;
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;

            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))){
                    frame->ip = ip;
                    runtimeError("Operand must be a number.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                push( NUMBER_VAL( -(AS_NUMBER(pop())) ) );
                break;
            
            case OP_PRINT:
                printValue(pop());
                printf("\n");
                break;

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
                // store ip from register to call frame (return address)
                frame->ip = ip;
                if (!callValue(peek(argCount), argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // new call frame on stack, new register pointer
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
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
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }

            case OP_CLASS: {
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))){
                    runtimeError("Only instances have properties.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(0));
                Value name = READ_CONSTANT();

                Value value;
                if (tableGet(&instance->fields, name, &value)){
                    pop();    // Instance
                    push(value);
                    break;
                }
                if (!bindMethod(instance->klass, name)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))){
                    runtimeError("Only instances have fields.");
                    return INTERPRETER_RUNTIME_ERROR;
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
            case OP_INVOKE: {
                Value method = READ_CONSTANT();
                int argCount = READ_BYTE();
                // store ip from register to call frame (return address)
                frame->ip = ip;
                if (!invoke(method, argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // update frame and ip
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)){
                    runtimeError("Superclass must be a class.");
                    return INTERPRETER_RUNTIME_ERROR;
                }
                ObjClass* subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                // Pop subclass. Superclass remains as local variable.
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
                // store ip from register to call frame (return address)
                frame->ip = ip;
                if (!invokeFromClass(superclass, method, argCount)){
                    return INTERPRETER_RUNTIME_ERROR;
                }
                // update frame and ip
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
        }    // end switch
    }        // end loop

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}
InterpreterResult interpret(const char* source, bool evalExpr){
    ObjFunction* topLevelCode = compile(source, evalExpr);
    if (topLevelCode == NULL)
        return INTERPRETER_COMPILE_ERROR;

    // push top-level call frame onto stack
    push(OBJ_VAL(topLevelCode));
    callFunction(topLevelCode, 0);

    return run();
}