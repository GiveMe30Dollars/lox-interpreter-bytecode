#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "debug.h"

// PRIVATE FUNCTIONS

typedef struct {
    Token prev;
    Token curr;
    bool hasError;
    bool panic;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,   // =
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

typedef void (*ParseFn)();
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
Chunk* compilingChunk;

static Chunk* currentChunk(){
    return compilingChunk;
}


// PARSER ERROR FUNCTIONS
static void errorAt(Token* token, const char* message){
    // if panic flag set, suppress all errors until resynchronized
    if (parser.panic) return;
    // set panic flag
    parser.panic = true;

    // print error line
    fprintf(stderr, "[line %d] Error", token->line);

    // Print position of token
    if (token->type == TOKEN_EOF){
        fprintf(stderr, "at end");
    } else if (token->type == TOKEN_ERROR){
        // print nothing; for scanner error tokens
    } else {
        // print token lexeme
        fprintf(stderr, "at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
}
static void errorAtCurrent(const char* message){
    errorAt(&parser.curr, message);
}
static void error(const char* message){
    errorAt(&parser.prev, message);
}


// PARSER HELPER FUNCTIONS
static void advance(){
    parser.prev = parser.curr;
    for (;;){
        parser.curr = scanToken();
        if (parser.curr.type != TOKEN_ERROR) break;
        // token is of type TOKEN_ERROR
        errorAtCurrent(parser.curr.start);
    }
}
static void consume(TokenType type, const char* message){
    if (parser.curr.type == type){
        advance();
        return;
    }
    errorAtCurrent(message);
}


// PARSER OUTPUT FUNCTIONS
static void emitByte(uint8_t byte){
    writeChunk(currentChunk(), byte, parser.prev.line);
}
static void emitBytes(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}
static uint8_t makeConstant(Value value){
    // stores the value in the current chunk's constants array, returns its index as a byte
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX){
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}
static void emitConstant(Value value){
    emitBytes(OP_CONSTANT, makeConstant(value));
}
static void emitReturn(){
    emitByte(OP_RETURN);
}
static void endCompiler(){
    emitReturn();
    #ifdef DEBUG_PRINT_CODE
    if (!parser.hasError){
        disassembleChunk(currentChunk(), "code");
    }
    #endif
}

// FORWARD DECLARATION OF PRATT-PARSER RELATED FUNCTIONS
static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);


// PARSER EXPRESSION FUNCTIONS
static void number(){
    double value = strtod(parser.prev.start, NULL);
    emitConstant(NUMBER_VAL(value));
}
static void grouping(){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    // No bytecode emitted.
}
static void unary(){
    // NOTE: for prefix unary operation
    TokenType operatorType = parser.prev.type;

    // Compile operand
    parsePrecedence(PREC_UNARY);
    
    // Emit instruction
    switch(operatorType){
        case TOKEN_BANG:    emitByte(OP_NOT); break;
        case TOKEN_MINUS:   emitByte(OP_NEGATE); break;
        default: return;    // Unreachable.
    }
}
static void binary(){
    TokenType operatorType = parser.prev.type;
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
static void literal(){
    switch(parser.prev.type){
        case TOKEN_NIL:   emitByte(OP_NIL); break;
        case TOKEN_TRUE:  emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        default: return;    // Unreachable
    }
}

static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}


// ParseRule table for Pratt Parsing
ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
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
    advance();
    ParseFn prefixRule = getRule(parser.prev.type)->prefix;
    // numbers, strings etc all have a prefix rule
    // tokens starting with no valid prefix rule are syntax errors
    if (prefixRule == NULL){
        error("Expect expression.");
        return;
    }
    // call prefix rule
    prefixRule();

    // while infix rule is of higher binding power than specified, execute infix rule
    while (precedence <= getRule(parser.curr.type)->precedence){
        advance();
        ParseFn infixRule = getRule(parser.prev.type)->infix;
        infixRule();
    }
}



// PUBLIC FUNCTIONS

bool compile(const char* source, Chunk* chunk){
    initScanner(source);
    parser.hasError = false;
    parser.panic = false;

    compilingChunk = chunk;

    // primes scanner such that current has a token
    advance();

    expression();

    consume(TOKEN_EOF, "Expect end of expression.");

    /*
    int line = -1;
    for (;;) {
        Token token = scanToken();
        if (token.line != line) {
        printf("%4d ", token.line);
        line = token.line;
        } else {
        printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start); 

        if (token.type == TOKEN_EOF) break;
    }*/

    endCompiler();

    return !parser.hasError;
}