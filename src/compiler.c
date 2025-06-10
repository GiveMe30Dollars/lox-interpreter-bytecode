#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
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
} Local;
typedef enum {
    TYPE_SCRIPT,
    TYPE_FUNCTION
} FunctionType;

typedef struct Compiler {
    FunctionType type;
    ObjFunction* function;

    struct Compiler* enclosing;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;

    HashTable existingConstants;
} Compiler;


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


// global variables
Parser parser;
Compiler* current;

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

    // if (parser.previous.type == TOKEN_EOF)
    //     printf("Advanced: Token-EOF\n");
    // else
    //     printf("Advanced: Token-%.*s\n", parser.previous.length, parser.previous.start);

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
static bool check (TokenType type){
    return parser.current.type == type;
}
static bool match (TokenType type){
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
static void emitConstant(Value value){
    emitBytes(OP_CONSTANT, makeConstant(value));
}
static void emitReturn(){
    emitByte(OP_NIL);
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


// Compiler constructor/destructor
static void initCompiler(Compiler* compiler, FunctionType type){
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    compiler->type = type;
    initTable(&compiler->existingConstants);

    // set this as current compiler
    compiler->enclosing = current;
    current = compiler;

    // if this is a function or class, get name from parser
    if (type != TYPE_SCRIPT){
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    // claim slot 0 of call stack for self
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
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


// FORWARD DECLARATION OF PRATT-PARSER RELATED FUNCTIONS
static void expression();
static void statement();
static void declaration();

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);


// PARSER EXPRESSION FUNCTIONS
static void number(bool canAssign){
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}
static void string(bool canAssign){
    // strip quotation marks in call to copyString()
    // (copyString() because there is no guarantee that the source string survives past compilation)
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
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
        case TOKEN_PLUS:    emitByte(OP_UNARY_PLUS); break;
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


// PARSER VARIABLE HELPER FUNCTIONS
static uint8_t identifierConstant(Token* name){
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
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
    emitBytes(OP_DEFINE_GLOBAL, global);
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
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    bool assigned = false;
    if (canAssign){
        int assignInstruction = -1;
        switch(parser.current.type){
            case (TOKEN_EQUAL): {
                // direct assignment
                // evaluate expression and set variable to value on stack
                advance();
                expression();
                emitBytes(setOp, (uint8_t)arg);
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
            emitBytes(setOp, arg);
            return;
        }
    }
    // Assumed get operation if no assignment succeeded
    emitBytes(getOp, (uint8_t)arg);

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
        pops++;
        current->localCount--;
    }
    switch (pops){
        case 0: break;     // Emit nothing
        case 1: emitByte(OP_POP); break;
        default:
            emitBytes(OP_POPN, pops);
            break;
    }
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

    if (match(TOKEN_ELSE)){
        // else branch.
        int elseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        emitByte(OP_POP);
        statement();
        patchJump(elseJump);
    } else {
        // no else branch. patch.
        patchJump(thenJump);
    }
}
static void whileStatement(){
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after if condition");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);
    
    patchJump(exitJump);
    emitByte(OP_POP);
}
static void forStatement(){
    // Looping variable belongs to scope of loop
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)){
        // No initializer
    } else if (match(TOKEN_VAR)){
        varDeclaration();
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
        incrStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);

        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        patchJump(bodyJump);
    }

    statement();
    emitLoop(incrStart);

    if (exitJump != -1){
        patchJump(exitJump);
    }
    // remember to pop the loop condition off the stack when exiting!
    emitByte(OP_POP);

    endScope();
}


static void function(FunctionType type){
    Compiler compiler;
    initCompiler(&compiler, TYPE_FUNCTION);

    // All parameters go into block scope.
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
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
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
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
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
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
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {unary,    binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},           // <- end of vanilla for single letter tokens

    [TOKEN_QUERY]         = {NULL,     conditional, PREC_CONDITIONAL},
    [TOKEN_COLON]         = {NULL,     NULL,        PREC_NONE},

    [TOKEN_BANG]          = {unary,    NULL,     PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},     // <- end of vanilla for single/double letter tokens

    [TOKEN_PLUS_EQUAL]    = {NULL,     NULL,   PREC_NONE},             // assignment is handled in variable.
    [TOKEN_MINUS_EQUAL]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STAR_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH_EQUAL]   = {NULL,     NULL,   PREC_NONE},

    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},


    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
static ParseRule* getRule(TokenType type){
    return &rules[type];
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
    if (canAssign && match(TOKEN_EQUAL)){
        error("Invalid assignment target.");
    }
}



// PUBLIC FUNCTIONS

ObjFunction* compile(const char* source){
    initScanner(source);

    // initialize parser
    parser.hasError = false;
    parser.panic = false;


    // initialize compiler
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    // primes scanner such that current has a token
    advance();
    
    while (!match(TOKEN_EOF)){
        declaration();
    }

    ObjFunction* function = endCompiler();
    return !parser.hasError ? function : NULL;
}