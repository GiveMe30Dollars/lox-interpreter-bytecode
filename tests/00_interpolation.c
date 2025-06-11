#include "scanner.h"

int main(int argc, const char *argv[]){
    char* tests[] = {
        "for while \"string\"  \"interpolate ${val}\" {} " ,
        "\"nested ${\"interpolation ${\"why?\"}\"}\"" ,
        "\"interpolation ${ fun(n) { \"nested ${ id {} }end\" } }end\"" ,
        "\"chained ${val1} with ${val2} and end.\"" ,
        "malformed: }",
        "{{{}}{}}  \"string ${\"${{{}{}\"${}\"}}\"}\""
    };

    for (int i = 0; i < 5; i++){
        initScanner(tests[i]);
        Token t;
        while ((t = scanToken()).type != TOKEN_EOF){
            printf("Token-%02d: %.*s\n", t.type, t.length, t.start);
        }
        printf("Token-46: EOL\n\n");
    }
}

/*
This script is a helper for testing the scanning of string interpolations.

Expect:

Token-33: for
Token-44: while
Token-26: string
Token-28: interpolate
Token-25: val
Token-26:
Token-02: {
Token-03: }
Token-46: EOL

Token-28: nested
Token-28: interpolation
Token-26: why?
Token-26:
Token-26:
Token-46: EOL

Token-28: interpolation
Token-34: fun
Token-00: (
Token-25: n
Token-01: )
Token-02: {
Token-28: nested
Token-25: id
Token-02: {
Token-03: }
Token-26: end
Token-03: }
Token-26: end
Token-46: EOL

Token-28: chained
Token-25: val1
Token-28:  with
Token-25: val2
Token-26:  and end.
Token-46: EOL

Token-25: malformed
Token-12: :
Token-45: Unmatched '}'.
Token-46: EOL

Token-02: {
Token-02: {
Token-02: {
Token-03: }
Token-03: }
Token-02: {
Token-03: }
Token-03: }
Token-28: string
Token-28:
Token-02: {
Token-02: {
Token-03: }
Token-02: {
Token-03: }
Token-28:
Token-26:
Token-03: }
Token-26:
Token-26:
Token-46: EOL

*/