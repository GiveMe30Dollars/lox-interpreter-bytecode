#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

// PRIVATE FUNCTIONS

// COMPILER STRUCTURES
typedef struct {
    Token previous;
    Token current;
    bool hasError;
    bool panic;
} Parser;


typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;
typedef enum {
    TYPE_SCRIPT,
    TYPE_FUNCTION,
    TYPE_LAMBDA,
    TYPE_METHOD,
    TYPE_INITIALIZER,
    TYPE_STATIC_METHOD,
    TYPE_TRY_BLOCK
} FunctionType;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

// For break and continue:
// A loop needs to keep track of its starting position for continue
// and all the indexes for jump operations to backpatch for break
// this includes the for condition (if exists)
// LoopInfo owns its endpatches array.
// It is never dynamically allocated, and instead created whenever a loop is created
// and deleted (out of scope) once the loop finishes compiling
typedef struct LoopInfo {
    int loopStart;
    int loopDepth;
    int loopVariable;
    int innerVariable;
    struct LoopInfo* enclosing;
    ValueArray endpatches;
} LoopInfo;


typedef struct Compiler {
    FunctionType type;
    ObjFunction* function;
    
    LoopInfo* loop;

    struct Compiler* enclosing;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    Upvalue upvalues[UINT8_COUNT];

    HashTable existingConstants;
} Compiler;

// All logic for handling this struct is in classDeclaration
typedef enum {
    SUPERCLASS_NONE,
    SUPERCLASS_SINGLE,
    SUPERCLASS_MULTIPLE
} SuperclassType;
typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    SuperclassType type;
} ClassCompiler;


typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,   // =
    PREC_CONDITIONAL,  // ?:
    PREC_OR,           // or
    PREC_AND,          // and
    PREC_EQUALITY,     // == !=
    PREC_COMPARISON,   // < > <= >=
    PREC_TERM,         // + -
    PREC_FACTOR,       // * /
    PREC_UNARY,        // ! -
    PREC_CALL,         // . ()
    PREC_PRIMARY
} Precedence;
// Precedence enum is ordered in ascending binding power

typedef void (*ParseFn)(bool canAssign);
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// Stores the parsing rules for each operator
// as a prefix or infix operator, then its precedence
// its assumed that the operator has the same precedence in any position


// GLOBAL VARIABLES
Parser parser;
Compiler* current;
ClassCompiler* currentClass;

static Chunk* currentChunk(){
    return &current->function->chunk;
}


// PARSER ERROR FUNCTIONS
static void errorAt(Token* token, const char* message){
    // if panic flag set, suppress all errors until resynchronized
    if (parser.panic) return;
    // set flags corresponding to error found
    parser.hasError = true;
    parser.panic = true;

    // print error line
    fprintf(stderr, "[line %d] Error", token->line);

    // Print position of token
    if (token->type == TOKEN_EOF){
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR){
        // print nothing; for scanner error tokens
    } else {
        // print token lexeme
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
}
static void errorAtCurrent(const char* message){
    errorAt(&parser.current, message);
}
static void error(const char* message){
    errorAt(&parser.previous, message);
}


// PARSER HELPER FUNCTIONS
static void advance(){
    parser.previous = parser.current;
    for (;;){
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        // token is of type TOKEN_ERROR
        errorAtCurrent(parser.current.start);
    }
}
static void consume(TokenType type, const char* message){
    if (parser.current.type == type){
        advance();
        return;
    }
    errorAtCurrent(message);
}
static bool check(TokenType type){
    return parser.current.type == type;
}
static bool match(TokenType type){
    if (!check(type)) return false;
    advance();
    return true;
}
static void synchronize(){
    parser.panic = false;
    while (parser.current.type != TOKEN_EOF){
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type){
            case TOKEN_PRINT:
            case TOKEN_VAR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_FUN:
            case TOKEN_CLASS:
                return;
            default: ;    // Nothing. fall through and advance.
        }
        advance();
    }
}
static inline bool isStatement(){
    // returns true if the current token matches a keyword that indicates the beginning of a statement
    switch(parser.current.type){
        case TOKEN_PRINT:
        case TOKEN_VAR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_FOR:
        case TOKEN_FUN:
        case TOKEN_CLASS:
        case TOKEN_RETURN:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
            return true;
        default: ;    // Fallthrough
    }
    return false;
}

static Token syntheticToken(const char* name){
    Token token;
    token.start = name;
    token.length = (int)strlen(name);
    return token;
}


// PARSER EMIT FUNCTIONS
static void emitByte(uint8_t byte){
    writeChunk(currentChunk(), byte, parser.previous.line);
}
static void emitBytes(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}
static uint8_t makeConstant(Value value){
    // stores the value in the current chunk's constants array, returns its index as a byte
    // if identifier already exists, return that instead
    Value idx;
    if (tableGet(&current->existingConstants, value, &idx))
        return (uint8_t)AS_NUMBER(idx);
    
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX){
        error("Too many constants in one chunk.");
        return 0;
    }
    tableSet(&current->existingConstants, value, NUMBER_VAL(constant));
    return (uint8_t)constant;
}
static void emitConstant(Opcode instruction, uint8_t operand){
    emitBytes(instruction, operand);
}
static void emitReturn(){
    // if initializer, return 'this' on reserved slot 0
    if (current->type == TYPE_INITIALIZER){
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
}
static int emitJump(uint8_t instruction){
    // emit a jump instruction with max offset. patch jump later.
    // returns ip offset to bytes to be patched
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}
static void patchJump(int offset){
    // -2 adjusts for the bytecode offset of the JUMP operand
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX){
        error("Too much bytecode to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}
static void emitLoop(int loopStart){
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}
static void emitPops(int number){
    // Assumes that number <= UINT8_MAX
    switch (number){
        case 0: break;     // Emit nothing
        case 1: emitByte(OP_POP); break;
        default:
            emitBytes(OP_POPN, number);
            break;
    }
}


// Compiler constructor/destructor
static void initCompiler(Compiler* compiler, FunctionType type){
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    compiler->type = type;
    initTable(&compiler->existingConstants);
    compiler->loop = NULL;

    // set this as current compiler
    compiler->enclosing = current;
    current = compiler;

    // if this is a function or class, get name from parser
    // if this is a lambda, generate one
    if (type == TYPE_LAMBDA){
        current->function->name = copyString("lambda", 6);
    } else if (type != TYPE_SCRIPT){
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    // claim slot 0 of call stack for self
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type == TYPE_METHOD || type == TYPE_INITIALIZER){
        // define 'this' for methods and initializers
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}
static ObjFunction* endCompiler(){
    // emit final byte, extract ObjFunction*
    emitReturn();
    ObjFunction* function = current->function;

    // clean up allocated compiler temporaries
    freeTable(&current->existingConstants);

    // restores enclosing compiler
    current = current->enclosing;
    
    #ifdef DEBUG_PRINT_CODE
    if (!parser.hasError){
        disassembleChunk(&function->chunk, (function->name != NULL ? function->name->chars : "<script>"));
    }
    #endif

    return function;
}

static void initLoopInfo(LoopInfo* loopInfo, int loopStart){
    initValueArray(&loopInfo->endpatches);
    loopInfo->loopStart = loopStart;
    loopInfo->loopDepth = current->scopeDepth;
    // initialize loop and inner variable to -1 (specific to for)
    loopInfo->loopVariable = -1;
    loopInfo->innerVariable = -1;
    loopInfo->enclosing = current->loop;
    // set this as current loop
    current->loop = loopInfo;
}
static void endLoopInfo(){
    // backpatch all recorded jumps to end of loop
    ValueArray* array = &current->loop->endpatches;
    for (int i = 0; i < array->count; i++){
        patchJump((int)AS_NUMBER(array->values[i]));
    }
    // free associated temporaries
    freeValueArray(&current->loop->endpatches);
    // restore enclosing loopInfo
    current->loop = current->loop->enclosing;
}
static void addEndpatch(int offset){
    // adds a jump instruction at this offset to be patched to the end of this loop
    writeValueArray(&current->loop->endpatches, NUMBER_VAL(offset));
}
static void emitPrejumpPops(){
    // iterates through locals (does not affect it!), emits pops for values within the loop
    // if the loop has a captured value, emit OP_CLOSE_UPVALUE instead
    // (scope depth higher but not including depth at which loop is created)
    int pops = 0;
    for (int i = current->localCount - 1; i >= 0; i--){
        if (current->locals[i].depth > current->loop->loopDepth){
            if (current->locals[i].isCaptured){
                emitPops(pops);
                pops = 0;
                emitByte(OP_CLOSE_UPVALUE);
            } else {
                pops++;
            }
        } else break;
    }
    emitPops(pops);
}


// FORWARD DECLARATION OF PRATT-PARSER RELATED FUNCTIONS
static void expression();
static void statement();
static void declaration();

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);


// PARSER EXPRESSION FUNCTIONS
static void number(bool canAssign){
    double value = strtod(parser.previous.start, NULL);
    uint8_t constant = makeConstant(NUMBER_VAL(value));
    emitConstant(OP_CONSTANT, constant);
}
static void string(bool canAssign){
    // quotation marks are already stripped (changed after string interpolation added)
    // (copyString() because there is no guarantee that the source string survives past compilation)
    uint8_t constant = makeConstant(OBJ_VAL(copyString(parser.previous.start, parser.previous.length)));
    emitConstant(OP_CONSTANT, constant);
}
static void grouping(bool canAssign){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    // No bytecode emitted.
}
static void unary(bool canAssign){
    // NOTE: for prefix unary operations
    TokenType operatorType = parser.previous.type;

    // Compile operand
    parsePrecedence(PREC_UNARY);
    
    // Emit instruction
    switch(operatorType){
        case TOKEN_BANG:    emitByte(OP_NOT); break;
        case TOKEN_MINUS:   emitByte(OP_NEGATE); break;
        case TOKEN_PLUS:    emitBytes(OP_NEGATE, OP_NEGATE); break;
        default: return;    // Unreachable.
    }
}
static void binary(bool canAssign){
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType){
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;

        case TOKEN_PLUS:    emitByte(OP_ADD); break;
        case TOKEN_MINUS:   emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:    emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:   emitByte(OP_DIVIDE); break;
    }
}
static void literal(bool canAssign){
    switch(parser.previous.type){
        case TOKEN_NIL:   emitByte(OP_NIL); break;
        case TOKEN_TRUE:  emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        default: return;    // Unreachable
    }
}
static void conditional(bool canAssign){
    // copies the control flow of if-then-else
    if (match(TOKEN_COLON)){
        // Elvis operator
        int thenJump = emitJump(OP_JUMP_IF_FALSE);
        int elseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        emitByte(OP_POP);
        parsePrecedence(PREC_CONDITIONAL);
        patchJump(elseJump);
    }
    else {
        // ternary conditional
        int thenJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
        parsePrecedence(PREC_CONDITIONAL);
        int elseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        
        consume(TOKEN_COLON, "Expect ':' after first branch of ternary conditional.");
        emitByte(OP_POP);
        parsePrecedence(PREC_CONDITIONAL);
        patchJump(elseJump);
    }
}
static void and_(bool canAssign){
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}
static void or_(bool canAssign){
    // cannot negate first operand for naive OP_JUMP_IF_FALSE,
    // two jump operations are always needed
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}


// PARSER VARIABLE HELPER FUNCTIONS
static uint8_t identifierConstant(Token* name){
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}
static uint8_t syntheticConstant(const char* name){
    Token synth = syntheticToken(name);
    return identifierConstant(&synth);
}

static bool identifiersEqual(Token* a, Token* b){
    if (a->length != b->length) return false;
    return (memcmp(a->start, b->start, a->length) == 0);
}
static void addLocal(Token name){
    if (current->localCount == UINT8_COUNT){
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}
static void markInitialized(){
    // specific to local variables
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}
static int resolveLocal(Compiler* compiler, Token* name){
    // returns the position of local variable on the stack.
    // if local variable does not exist, return -1
    // (treat as global variable)
    for (int i = compiler->localCount - 1; i >= 0; i--){
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)){
            if (local->depth == -1){
                error("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, int slotIdx, bool isLocal){
    int upvalueCount = compiler->function->upvalueCount;
    for (int i = 0; i < upvalueCount; i++){
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == slotIdx && upvalue->isLocal == isLocal)
            return i;
    }
    if (upvalueCount == UINT8_MAX){
        error("Too many closure variables in function.");
        return 0;
    }
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = slotIdx;
    return compiler->function->upvalueCount++;
}
static int resolveUpvalue(Compiler* compiler, Token* name){
    // resolves an upvalue in the enclosing compiler

    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1){
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    // Recursive case: if found, add to this compiler as nonlocal upvalue
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1){
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void declareVariable(){
    // specific to local variables
    // asserts no existing variable in this scope has the same name, then adds to locals
    if (current->scopeDepth == 0) return;
    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--){
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth){
            break;
        }
        if (identifiersEqual(name, &local->name)){
            error("Already a variable with this name in this scope.");
            return;
        }
    }
    addLocal(*name);
}
static void defineVariable(uint8_t global){
    // specific to global variables
    // local variables are on the stack. mark slot as initialized
    if (current->scopeDepth > 0){
        markInitialized();
        return;
    }
    emitConstant(OP_DEFINE_GLOBAL, global);
}
static uint8_t parseVariable(const char* errorMessage){
    // parses the variable name, then:
    // global scope: returns the constant idx to the variable name
    // local scope: declares the variable name in the Compiler locals struct
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable();
    if (current->scopeDepth > 0) return 0;
    return identifierConstant(&parser.previous);
}


static void namedVariable(Token name, bool canAssign){
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1){
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1){
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign){
        int assignInstruction = -1;
        switch(parser.current.type){
            case (TOKEN_EQUAL): {
                // direct assignment
                // evaluate expression and set variable to value on stack
                advance();
                expression();
                emitConstant(setOp, (uint8_t)arg);
                return;
            }
            case (TOKEN_PLUS_EQUAL):   assignInstruction = OP_ADD; break;
            case (TOKEN_MINUS_EQUAL):  assignInstruction = OP_SUBTRACT; break;
            case (TOKEN_STAR_EQUAL):   assignInstruction = OP_MULTIPLY; break;
            case (TOKEN_SLASH_EQUAL):  assignInstruction = OP_DIVIDE; break;
            default: ;    // Nothing.
        }
        if (assignInstruction != -1){
            // compound assignment operation.
            // get variable, advance past operator, evaluate expression to the right (PREC_ASSIGN)
            // apply operation and set variable to value on stack
            emitBytes(getOp, arg);
            advance();
            expression();
            emitByte((uint8_t)assignInstruction);
            emitConstant(setOp, arg);
            return;
        }
    }
    // Assumed get operation if no assignment succeeded
    emitConstant(getOp, arg);

    // if variable assignment on an invalid target, return to parsePrecedence and do not consume '='
    // error handling is done there.
}
static void variable(bool canAssign){
    // for variable get and set
    namedVariable(parser.previous, canAssign);
}

static uint8_t argumentList(){
    // Evaluates all arguments to be on the stack.
    // LEFT_PAREN already consumed
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)){
        do {
            expression();
            if (argCount == 255){
                // About to overflow to 256 arguments
                error("Cannot have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}
static void call(bool canAssign){
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}


static void interpolation(bool canAssign){
    // String interpolation handling

    // Get the index to "string" and "concatenate" constant (required to call native function)
    uint8_t idxS = makeConstant(OBJ_VAL(copyString("string", 6)));
    uint8_t idxC = makeConstant(OBJ_VAL(copyString("concatenate", 11)));

    // get concatenate native function onto stack
    emitConstant(OP_GET_STL, idxC);

    int argCount = 0;
    do {
        // pass the TOKEN_INTERPOLATION to be parsed as a string
        string(canAssign);

        // get the Lox stringcast native function, evaluate the expression, and call it
        emitConstant(OP_GET_STL, idxS);
        expression();
        emitBytes(OP_CALL, 1);

        // increment argCount
        if (argCount > UINT8_MAX - 2){
            error("Cannot exceed 128 chained string interpolations.");
        }
        argCount += 2;
    } while (match(TOKEN_INTERPOLATION));

    // parse ending string
    advance();
    string(canAssign);
    argCount++;

    emitBytes(OP_CALL, (uint8_t)argCount);
}

static void dot(bool canAssign){
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)){
        expression();
        emitConstant(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)){
        // Optimized invocations
        uint8_t argCount = argumentList();
        emitConstant(OP_INVOKE, name);
        emitByte(argCount);
    } else {
        emitConstant(OP_GET_PROPERTY, name);
    }
}

static void array(bool canAssign){
    uint8_t idxArray = syntheticConstant("Array");
    uint8_t idxRaw = syntheticConstant("@raw");
    emitConstant(OP_GET_STL, idxArray);
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET)){
        do {
            expression();
            if (argCount == 255){
                // About to overflow to 256 arguments
                error("Cannot have more than 255 elements in array literal.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array contents.");
    emitConstant(OP_INVOKE, idxRaw);
    emitByte(argCount);
}

static void subscript(bool canAssign){
    uint8_t idxGet = syntheticConstant("get");
    uint8_t idxSet = syntheticConstant("set");

    // left bracket already consumed
    parsePrecedence(PREC_CONDITIONAL);
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");

    if (canAssign){
        int assignInstruction = -1;
        switch(parser.current.type){
            case (TOKEN_EQUAL): {
                // direct assignment
                // evaluate expression and set variable to value on stack
                advance();
                expression();
                emitConstant(OP_INVOKE, idxSet);
                emitByte(2);
                return;
            }
            case (TOKEN_PLUS_EQUAL):   assignInstruction = OP_ADD; break;
            case (TOKEN_MINUS_EQUAL):  assignInstruction = OP_SUBTRACT; break;
            case (TOKEN_STAR_EQUAL):   assignInstruction = OP_MULTIPLY; break;
            case (TOKEN_SLASH_EQUAL):  assignInstruction = OP_DIVIDE; break;
            default: ;    // Nothing.
        }
        if (assignInstruction != -1){
            // compound assignment operation.
            // get variable, advance past operator, evaluate expression to the right (PREC_ASSIGN)
            // apply operation and set variable to value on stack
            emitBytes(OP_DUPLICATE, 1);
            emitBytes(OP_DUPLICATE, 1);
            emitBytes(OP_INVOKE, idxGet);
            emitByte(1);

            advance();
            expression();
            emitByte((uint8_t)assignInstruction);

            emitConstant(OP_INVOKE, idxSet);
            emitByte(2);
            return;
        }
    }
    // else, treat as getter
    emitConstant(OP_INVOKE, idxGet);
    emitByte(1);
}


static bool tryParseExpression(TokenType terminator){
    // attempts to parse as expression
    // if isStatement, parse nothing, return false
    // if is expression followed by terminator, return true
    // if is expression not followed by terminator (usually ';'),
    // parse as expresison statement and return false.
    if (isStatement()) return false;
    expression();
    if (match(terminator)) return true;
    else {
        consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
        emitByte(OP_POP);
        return false;
    }
}


// PARSER STATEMENT FUNCTIONS
static void varDeclaration(){
    uint8_t global = parseVariable("Expect variable name.");

    // Evaluate and emit bytecode for initialization value first
    if (match(TOKEN_EQUAL)){
        expression();
    } else {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
}

static void printStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_PRINT);
}
static void exprStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}


static void beginScope(){
    current->scopeDepth++;
}
static void endScope(){
    current->scopeDepth--;
    int pops = 0;
    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth){
        if (current->locals[current->localCount - 1].isCaptured){
            // first emit the number of pops required, then reset the number of pops
            emitPops(pops);
            pops = 0;
            // then close the upvalue
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            pops++;
        }
        current->localCount--;
    }
    emitPops(pops);
}
static void block(){
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
        declaration();
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}


static void ifStatement(){
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    // then branch;
    emitByte(OP_POP);
    statement();
    int elseJump = emitJump(OP_JUMP);

    // else branch.
    patchJump(thenJump);
    emitByte(OP_POP);
    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}
static void whileStatement(){
    int loopStart = currentChunk()->count;
    LoopInfo loop;
    initLoopInfo(&loop, loopStart);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after if condition");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);
    
    patchJump(exitJump);
    emitByte(OP_POP);

    endLoopInfo();
}
static void forStatement(){
    // Looping variable belongs to scope of loop
    beginScope();

    // Loop variable is initialized with no name
    // the accessible inner variable is in the loop block scope
    int loopVariable = -1;
    Token loopVariableName;
    loopVariableName.start = NULL;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)){
        // No initializer
    } else if (match(TOKEN_VAR)){
        // Save name and position (must be local variable)
        loopVariableName = parser.current;
        varDeclaration();
        loopVariable = current->localCount - 1;
    } else {
        exprStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (match(TOKEN_SEMICOLON)){
        // No terminating condition
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    int incrStart = loopStart;
    if  (!match(TOKEN_RIGHT_PAREN)){
        // increment clause. evaluate and pop result (treat as statement)
        int bodyJump = emitJump(OP_JUMP);
        // Overwrite 'start of loop' to be at the increment clause
        incrStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);

        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        patchJump(bodyJump);
    }

    // create inner variable if loop variable declared
    // with value of loop variable
    // create new scope to contain it
    int innerVariable = -1;
    if (loopVariable != -1){
        beginScope();
        emitBytes(OP_GET_LOCAL, loopVariable);
        addLocal(loopVariableName);
        markInitialized();
        innerVariable = current->localCount - 1;
    }

    LoopInfo loop;
    initLoopInfo(&loop, incrStart);
    // save loop and inner variable information
    loop.loopVariable = loopVariable;
    loop.innerVariable = innerVariable;

    statement();

    if (loopVariable != -1){
        // set the value of the loop variable to the inner variable at end of statement
        // then loop to increment clause
        emitBytes(OP_GET_LOCAL, (uint8_t)innerVariable);
        emitBytes(OP_SET_LOCAL, (uint8_t)loopVariable);
        emitByte(OP_POP);
        // don't forget to close scope opened when inner variable made!
        endScope();
    }

    emitLoop(incrStart);

    if (exitJump != -1){
        patchJump(exitJump);
    }
    // remember to pop the loop condition off the stack when exiting!
    emitByte(OP_POP);

    endLoopInfo();

    endScope();
}

static void breakStatement(){
    if (current->loop == NULL)
        error("Cannot use 'break' when not in loop.");
    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    // No need to assign to loop variable since we're breaking the loop.
    emitPrejumpPops();

    // remove inner variable (if any): OP_CLOSE_UPVALUE if captured, OP_POP otherwise
    if (current->loop->loopVariable != -1)
        emitByte(current->locals[current->localCount - 1].isCaptured ? OP_CLOSE_UPVALUE : OP_POP);
    
    // emit jump to endpatch
    int offset = emitJump(OP_JUMP);
    addEndpatch(offset);
}
static void continueStatement(){
    if (current->loop == NULL)
        error("Cannot use 'continue' when not in loop.");
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    // assign inner variable to loop variable (if any)
    if (current->loop->loopVariable != -1){
        emitBytes(OP_GET_LOCAL, (uint8_t)current->loop->innerVariable);
        emitBytes(OP_SET_LOCAL, (uint8_t)current->loop->loopVariable);
        emitByte(OP_POP);
    }

    emitPrejumpPops();

    // remove inner variable (if any): OP_CLOSE_UPVALUE if captured, OP_POP otherwise
    if (current->loop->loopVariable != -1)
        emitByte(current->locals[current->localCount - 1].isCaptured ? OP_CLOSE_UPVALUE : OP_POP);

    // emit jump to loop start
    emitLoop(current->loop->loopStart);
}


static void function(FunctionType type){
    Compiler compiler;
    initCompiler(&compiler, type);

    // All parameters go into block scope.
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name or 'fun'.");
    if (!check(TOKEN_RIGHT_PAREN)){
        do {
            current->function->arity++;
            if (current->function->arity > 255){
                error("Cannot have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    if ((type == TYPE_LAMBDA) && tryParseExpression(TOKEN_RIGHT_BRACE)){
        emitByte(OP_RETURN);
    } else {
        block();
    }

    ObjFunction* function = endCompiler();

    if (function->upvalueCount > 0){
        // create closure object
        emitConstant(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
        for (int i = 0; i < function->upvalueCount; i++){
            emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
            emitByte(compiler.upvalues[i].index);
        }
    } else {
        // create function object
        emitConstant(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
    }
}
static void functionDeclaration(){
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}
static void returnStatement(){
    // disallow return statement from top-level code
    // (internally, we exit the VM by emitting OP_RETURN at EOF)
    if (current->type == TYPE_SCRIPT){
        error("Cannot return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)){
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER){
            error("Cannot return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void lambda(bool canAssign){
    // Function literal with randomly-generated name
    // We came here from the Pratt parser
    function(TYPE_LAMBDA);
}

static void method(FunctionType type){
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t nameConstant = identifierConstant(&parser.previous);
    FunctionType funcType = type;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0){
        if (type == TYPE_STATIC_METHOD)
            error("Initializer cannot be declared as 'static'.");
        else funcType = TYPE_INITIALIZER;
    }
    function(funcType);
    Opcode instruction = (funcType == TYPE_STATIC_METHOD) ? OP_STATIC_METHOD : OP_METHOD;
    emitConstant(instruction, nameConstant);
}
static void classDeclaration(){
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitConstant(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    classCompiler.type = SUPERCLASS_NONE;
    currentClass = &classCompiler;

    // if there is a superclass
    if (match(TOKEN_LESS)){
        Opcode instruction;
        if (match(TOKEN_LEFT_BRACKET)){
            // multiple inheritance
            uint8_t idxArray = syntheticConstant("Array");
            uint8_t idxRaw = syntheticConstant("@raw");
            emitConstant(OP_GET_STL, idxArray);

            uint8_t argCount = 0;
            do {
                consume(TOKEN_IDENTIFIER, "Expect superclass name.");
                variable(false);
                if (argCount == 255){
                    // About to overflow to 256 arguments
                    error("Cannot have more than 255 elements in array literal.");
                } else argCount++;
                if (identifiersEqual(&parser.previous, &className))
                    error("A class cannot inherit itself.");
            } while (match(TOKEN_COMMA));
            consume(TOKEN_RIGHT_BRACKET, "Expect ']' after superclass array.");

            emitConstant(OP_INVOKE, idxRaw);
            emitByte(argCount);
            
            classCompiler.type = SUPERCLASS_MULTIPLE;
            instruction = OP_INHERIT_MULTIPLE;
        } else {
            // single inheritance
            consume(TOKEN_IDENTIFIER, "Expect superclass name.");
            variable(false);
            if (identifiersEqual(&parser.previous, &className))
                error("A class cannot inherit itself.");

            classCompiler.type = SUPERCLASS_SINGLE;
            instruction = OP_INHERIT;
        }

        // store superclass expression as 'super'
        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        // load class back onto stack for inheritance
        namedVariable(className, false);
        emitByte(instruction);
    }

    // load class back onto stack for method loading
    namedVariable(className, false);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)){
        if (match(TOKEN_STATIC)) method(TYPE_STATIC_METHOD);
        else method(TYPE_METHOD);
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    // pop class object
    emitByte(OP_POP);

    // end scope to pop superclass, if any
    if (classCompiler.type != SUPERCLASS_NONE)
        endScope();

    currentClass = currentClass->enclosing;
}
static void this_ (bool canAssign){
    if (currentClass == NULL){
        error("Cannot use 'this' outside a class.");
        return;
    }
    if (current->type == TYPE_STATIC_METHOD){
        error("Cannot use 'this' in a static method.");
        return;
    }
    // treat as local variable
    variable(false);
}
static void super_ (bool canAssign){
    if (currentClass == NULL){
        error("Cannot use 'super' outside of a class.");
    } else if (currentClass->type == SUPERCLASS_NONE){
        error("Cannot use 'super' in a class without a superclass.");
    } else if (current->type == TYPE_STATIC_METHOD){
        error("Cannot use 'super' in a static method.");
    }

    int superjump = emitJump(OP_JUMP);
    int superstart = currentChunk()->count;
    namedVariable(parser.previous, false);
    if (currentClass->type == SUPERCLASS_MULTIPLE){
        consume(TOKEN_LEFT_BRACKET, "Expect '[' after 'super' for multiple inheritance.");
        expression();
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");
        uint8_t idxGet = syntheticConstant("get");
        emitConstant(OP_INVOKE, idxGet);
        emitByte(1);
    }
    int returnJump = emitJump(OP_JUMP);
    patchJump(superjump);

    consume(TOKEN_DOT, "Expect '.' after super expression.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)){
        uint8_t argCount = argumentList();
        emitLoop(superstart);
        patchJump(returnJump);
        emitConstant(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        emitLoop(superstart);
        patchJump(returnJump);
        emitConstant(OP_GET_SUPER, name);
    }
}


static void tryStatement(){
    Compiler compiler;
    initCompiler(&compiler, TYPE_TRY_BLOCK);
    
    consume(TOKEN_LEFT_BRACE, "Expect '{' after 'try'.");

    // Everything goes into block scope
    beginScope();
    block();
    ObjFunction* function = endCompiler();
    function->fromTry = true;

    if (function->upvalueCount > 0){
        // create closure object
        emitConstant(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
        for (int i = 0; i < function->upvalueCount; i++){
            emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
            emitByte(compiler.upvalues[i].index);
        }
    } else {
        // create function object
        emitConstant(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
    }

    // Function/closure object is on the stack. Try call
    emitByte(OP_TRY_CALL);

    // Emit pop and jump operand (will be skipped if exception thrown)
    emitByte(OP_POP);
    int catchJump = emitJump(OP_JUMP);

    // Parse catch clause (no need to parse as function)
    beginScope();
    consume(TOKEN_CATCH, "Expect 'catch' after try clause.");
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'catch'.");
    uint8_t localIdx = parseVariable("Expect variable name.");
    defineVariable(localIdx);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after identifier.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before catch block.");
    block();
    endScope();

    patchJump(catchJump);
}
static void throwStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after throw statement.");
    emitByte(OP_THROW);
}




static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}
static void statement(){
    if (match(TOKEN_PRINT)){
        printStatement();
    } else if (match(TOKEN_RETURN)){
        returnStatement();
    } else if (match(TOKEN_IF)){
        ifStatement();
    } else if (match(TOKEN_WHILE)){
        whileStatement();
    } else if (match(TOKEN_FOR)){
        forStatement();
    } else if (match(TOKEN_BREAK)){
        breakStatement();
    } else if (match(TOKEN_CONTINUE)){
        continueStatement();
    } else if (match(TOKEN_TRY)) {
        tryStatement();
    } else if (match(TOKEN_THROW)){
        throwStatement();
    } else if (match(TOKEN_LEFT_BRACE)){
        beginScope();
        block();
        endScope();
    }
    else exprStatement();
}
static void declaration(){
    if (match(TOKEN_FUN)){
        functionDeclaration();
    } else if (match(TOKEN_CLASS)){
        classDeclaration();
    } else if (match(TOKEN_VAR)){
        varDeclaration();
    } else {
        statement();
    }
}


// ParseRule table for Pratt Parsing
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},

    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {unary,    binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},           // <- end of vanilla for single letter tokens

    [TOKEN_QUERY]         = {NULL,     conditional, PREC_CONDITIONAL},
    [TOKEN_COLON]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {array,    subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,        PREC_NONE},

    [TOKEN_BANG]          = {unary,    NULL,     PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},     // <- end of vanilla for single/double letter tokens

    [TOKEN_PLUS_EQUAL]    = {NULL,     NULL,   PREC_NONE},             // assignment is handled in variable()
    [TOKEN_MINUS_EQUAL]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STAR_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH_EQUAL]   = {NULL,     NULL,   PREC_NONE},

    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},             // <- end of vanilla

    [TOKEN_INTERPOLATION] = {interpolation, NULL,   PREC_NONE},

    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {lambda,   NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {super_,   NULL,   PREC_NONE},
    [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},

    [TOKEN_BREAK]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CONTINUE]      = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STATIC]        = {NULL,     NULL,   PREC_NONE},

    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
static ParseRule* getRule(TokenType type){
    return &rules[type];
}

static inline bool isAssignment(Token* token){
    switch(token->type){
        case TOKEN_EQUAL:
        case TOKEN_PLUS_EQUAL:
        case TOKEN_MINUS_EQUAL:
        case TOKEN_STAR_EQUAL:
        case TOKEN_SLASH_EQUAL:
            return true;
        default: return false;
    }
}

// PRATT PARSER
static void parsePrecedence(Precedence precedence){

    // advance and get the prefix rule of the token we advanced past
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    // numbers, strings etc all have a prefix rule
    // expressions starting with no valid prefix rule are syntax errors
    if (prefixRule == NULL){
        error("Expect expression.");
        return;
    }
    // call prefix rule.
    // canAssign is passed through for variable assignment
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    // while infix rule is of higher binding power than specified, execute infix rule
    while (precedence <= getRule(parser.current.type)->precedence){
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    // error handling for invalid variable assignment
    if (canAssign && isAssignment(&parser.current)){
        errorAtCurrent("Invalid assignment target.");
    }
}



// PUBLIC FUNCTIONS

ObjFunction* compile(const char* source, bool evalExpr){
    initScanner(source);

    // initialize parser
    parser.hasError = false;
    parser.panic = false;


    // initialize compiler
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    // primes scanner such that parser.current has a token
    advance();
    
    if (evalExpr && tryParseExpression(TOKEN_EOF)){
        // expression, value is currently on stack
        // print only if not nil

        // duplicate and test for equality with OP_NIL
        emitBytes(OP_DUPLICATE, 0);
        emitByte(OP_NIL);
        emitByte(OP_EQUAL);

        // then clause
        int thenJump = emitJump(OP_JUMP_IF_FALSE);
        emitBytes(OP_POPN, 2);
        int elseJump = emitJump(OP_JUMP);

        // else clause
        patchJump(thenJump);
        emitByte(OP_POP);
        emitByte(OP_PRINT);
        patchJump(elseJump);
    }
    while (!match(TOKEN_EOF)){
        declaration();
    }

    ObjFunction* function = endCompiler();
    return !parser.hasError ? function : NULL;
}


void markCompilerRoots(){
    // mark all compiling functions
    for (Compiler* compiler = current; compiler != NULL; compiler = compiler->enclosing){
        markObject((Obj*)compiler->function);
    }
    // the locals array does not need to be marked.
}