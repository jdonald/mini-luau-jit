// Hand-written lexer (replacement for flex-generated lexer.yy.cpp)
// Compatible with bison-generated parser.tab.hpp
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include "ast.h"
#include "parser.tab.hpp"

FILE* yyin = nullptr;
int yylineno = 1;
static char yytext_buffer[4096];
char* yytext = yytext_buffer;

extern YYSTYPE yylval;

static int current_char = -2; // -2 = uninitialized

static int next_char() {
    if (!yyin) return EOF;
    return fgetc(yyin);
}

static int peek_char() {
    if (current_char == -2) {
        current_char = next_char();
    }
    return current_char;
}

static void consume_char() {
    current_char = -2;
}

static void skip_whitespace() {
    while (true) {
        int c = peek_char();
        if (c == ' ' || c == '\t' || c == '\r') {
            consume_char();
        } else if (c == '\n') {
            consume_char();
            yylineno++;
        } else {
            break;
        }
    }
}

static void skip_line_comment() {
    // Already consumed first '-', check for second
    int c = peek_char();
    if (c == '-') {
        consume_char();
        // Skip to end of line
        while ((c = peek_char()) != EOF && c != '\n') {
            consume_char();
        }
    }
}

static int scan_number() {
    int pos = 0;

    while (true) {
        int c = peek_char();
        if (isdigit(c)) {
            if (pos < 4094) yytext[pos++] = c;
            consume_char();
        } else {
            break;
        }
    }
    yytext[pos] = '\0';
    yylval.ival = atoll(yytext);
    return INTEGER;
}

static int scan_identifier_or_keyword() {
    int pos = 0;

    while (true) {
        int c = peek_char();
        if (isalnum(c) || c == '_') {
            if (pos < 4094) yytext[pos++] = c;
            consume_char();
        } else {
            break;
        }
    }
    yytext[pos] = '\0';

    // Check for keywords
    if (strcmp(yytext, "function") == 0) return FUNCTION;
    if (strcmp(yytext, "end") == 0) return END;
    if (strcmp(yytext, "if") == 0) return IF;
    if (strcmp(yytext, "then") == 0) return THEN;
    if (strcmp(yytext, "else") == 0) return ELSE;
    if (strcmp(yytext, "elseif") == 0) return ELSEIF;
    if (strcmp(yytext, "while") == 0) return WHILE;
    if (strcmp(yytext, "do") == 0) return DO;
    if (strcmp(yytext, "return") == 0) return RETURN;
    if (strcmp(yytext, "local") == 0) return LOCAL;
    if (strcmp(yytext, "and") == 0) return AND;
    if (strcmp(yytext, "or") == 0) return OR;
    if (strcmp(yytext, "not") == 0) return NOT;
    if (strcmp(yytext, "type") == 0) return TYPE;
    if (strcmp(yytext, "print") == 0) return PRINT;
    if (strcmp(yytext, "true") == 0) {
        yylval.bval = true;
        return BOOLEAN;
    }
    if (strcmp(yytext, "false") == 0) {
        yylval.bval = false;
        return BOOLEAN;
    }

    // It's an identifier
    yylval.sval = strdup(yytext);
    return IDENTIFIER;
}

static int scan_string() {
    int pos = 0;

    consume_char(); // consume opening quote

    while (true) {
        int c = peek_char();
        if (c == EOF || c == '\n') {
            // Unterminated string
            yytext[pos] = '\0';
            yylval.sval = strdup(yytext);
            return STRING;
        }
        consume_char();

        if (c == '"') {
            // End of string
            break;
        }
        if (c == '\\') {
            // Escape sequence
            int escaped = peek_char();
            if (escaped != EOF) {
                consume_char();
                switch (escaped) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    default: c = escaped; break;
                }
            }
        }
        if (pos < 4094) yytext[pos++] = c;
    }
    yytext[pos] = '\0';
    yylval.sval = strdup(yytext);
    return STRING;
}

int yylex() {
    while (true) {
        skip_whitespace();

        int c = peek_char();

        if (c == EOF) {
            return 0; // End of input
        }

        // Single character tokens
        if (c == '+' || c == '*' || c == '/' || c == '%' ||
            c == '(' || c == ')' || c == ',' || c == ':') {
            consume_char();
            yytext[0] = c;
            yytext[1] = '\0';
            return c;
        }

        // Minus or comment
        if (c == '-') {
            consume_char();
            if (peek_char() == '-') {
                skip_line_comment();
                continue;
            }
            yytext[0] = '-';
            yytext[1] = '\0';
            return '-';
        }

        // Equals or comparison
        if (c == '=') {
            consume_char();
            if (peek_char() == '=') {
                consume_char();
                yytext[0] = '=';
                yytext[1] = '=';
                yytext[2] = '\0';
                return EQ;
            }
            yytext[0] = '=';
            yytext[1] = '\0';
            return '=';
        }

        // Not equals
        if (c == '~') {
            consume_char();
            if (peek_char() == '=') {
                consume_char();
                yytext[0] = '~';
                yytext[1] = '=';
                yytext[2] = '\0';
                return NE;
            }
            // Unknown token, skip
            continue;
        }

        // Less than or less equal
        if (c == '<') {
            consume_char();
            if (peek_char() == '=') {
                consume_char();
                yytext[0] = '<';
                yytext[1] = '=';
                yytext[2] = '\0';
                return LE;
            }
            yytext[0] = '<';
            yytext[1] = '\0';
            return LT;
        }

        // Greater than or greater equal
        if (c == '>') {
            consume_char();
            if (peek_char() == '=') {
                consume_char();
                yytext[0] = '>';
                yytext[1] = '=';
                yytext[2] = '\0';
                return GE;
            }
            yytext[0] = '>';
            yytext[1] = '\0';
            return GT;
        }

        // Numbers
        if (isdigit(c)) {
            return scan_number();
        }

        // Identifiers and keywords
        if (isalpha(c) || c == '_') {
            return scan_identifier_or_keyword();
        }

        // Strings
        if (c == '"') {
            return scan_string();
        }

        // Unknown character, skip
        consume_char();
    }
}

// yyerror is defined in parser.tab.cpp
