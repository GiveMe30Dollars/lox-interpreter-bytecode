#include <stdio.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

// PRIVATE FUNCTIONS

typedef struct {
    const char* start;
    const char* curr;
    int line;
} Scanner;

Scanner scanner;

// SCANNING FUNCTIONS
static bool isAtEnd(){
    return *scanner.curr == '\0';
}
static char advance(){
    scanner.curr++;
    return *(scanner.curr - 1);
}
static bool match(char expected){
    if (isAtEnd()) return false;
    if (*scanner.curr != expected) return false;
    scanner.curr++;
    return true;
}
static char peek(){
    return *scanner.curr;
}
static char peekNext(){
    if (isAtEnd()) return '\0';
    return *(scanner.curr + 1);
}

// CHAR HELPER FUNCTIONS
static bool isDigit(char c){
    return c >= '0' && c <= '9';
}
static bool isAlpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// TOKEN MAKING FUNCTIONS
static Token makeToken(TokenType type){
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.curr - scanner.start);
    token.line = scanner.line;
    return token;
}
static Token errorToken(const char* message){
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static void skipWhitespace(){
    for (;;){
        char c = peek();
        switch(c){
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;

            case '/':
                if (peekNext() == '/'){
                    while (peek() != '\n' && !isAtEnd()) advance();
                }
                else {
                    // TOKEN_SLASH found, not comment. return
                    return;
                }
                break;
                
            default:
                return;
        }
    }
}
static Token scanString(){
    while (peek() != '"' && !isAtEnd()){
        if (peek() == '\n') scanner.line++;
        advance();
    }
    if (isAtEnd()) return errorToken("Unterminated string.");

    // advance past the closing quote
    advance();

    return makeToken(TOKEN_STRING);
}
static Token scanNumber(){
    // get all digits
    while (isDigit(peek())) advance();
    
    // look for decimal. if exists, get trailing digits
    if (peek() == '.' && isDigit(peekNext())){
        // consume '.'
        advance();
        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type){
    // returns the TokenType specified if and only if the rest of the lexeme matches exactly
    if (scanner.curr - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) return type;
    return TOKEN_IDENTIFIER;
}
static TokenType identifierType(){
    // returns the token type of either a user-defined identifier or a reserved keyword
    switch(scanner.start[0]){
        // Lone branches
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);

        // Multiple branches
        case 'f':
            if (scanner.curr - scanner.start > 1){
                switch(scanner.start[1]){
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 't':
            if (scanner.curr - scanner.start > 1){
                switch(scanner.start[1]){
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
    }
    return TOKEN_IDENTIFIER;
}
static Token scanIdentifier(){
    // get all alphanumeric characters
    while (isDigit(peek()) || isAlpha(peek())) advance();
    return makeToken(identifierType());
}


// PUBLIC FUNCTIONS
void initScanner(const char* source){
    scanner.start = source;
    scanner.curr = source;
    scanner.line = 1;

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
    }
}

Token scanToken(){
    skipWhitespace();
    scanner.start = scanner.curr;
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    // Number literal
    if (isDigit(c)) return scanNumber();
    // Indentifiers and reserved keywords
    if (isAlpha(c)) return scanIdentifier();

    switch(c){
        // single-character tokens
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '+': return makeToken(TOKEN_PLUS);
        case '-': return makeToken(TOKEN_MINUS);
        case '*': return makeToken(TOKEN_STAR);
        case '/': return makeToken(TOKEN_SLASH);

        // Single or double character tokens
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

        // String literal
        case '"': return scanString();

        default:
            return errorToken("Unidenfied character.");
    }
}