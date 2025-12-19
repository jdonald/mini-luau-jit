%{
#include <string>
#include "ast.h"
#include "parser.tab.hpp"

extern int yylineno;
%}

%option noyywrap
%option yylineno

%%

[ \t\r]+              { /* Skip whitespace */ }
\n                    { /* Skip newlines */ }
"--".*                { /* Skip single-line comments */ }

"function"            { return FUNCTION; }
"end"                 { return END; }
"if"                  { return IF; }
"then"                { return THEN; }
"else"                { return ELSE; }
"elseif"              { return ELSEIF; }
"while"               { return WHILE; }
"do"                  { return DO; }
"return"              { return RETURN; }
"local"               { return LOCAL; }
"true"                { yylval.bval = true; return BOOLEAN; }
"false"               { yylval.bval = false; return BOOLEAN; }
"and"                 { return AND; }
"or"                  { return OR; }
"not"                 { return NOT; }
"type"                { return TYPE; }
"print"               { return PRINT; }

"=="                  { return EQ; }
"~="                  { return NE; }
"<="                  { return LE; }
">="                  { return GE; }
"<"                   { return LT; }
">"                   { return GT; }

[0-9]+                { yylval.ival = atoll(yytext); return INTEGER; }
[a-zA-Z_][a-zA-Z0-9_]* { yylval.sval = strdup(yytext); return IDENTIFIER; }
\"([^\\\"]|\\.)*\"    {
                        yylval.sval = strdup(yytext + 1);
                        yylval.sval[strlen(yylval.sval) - 1] = '\0';
                        return STRING;
                      }

"+"                   { return '+'; }
"-"                   { return '-'; }
"*"                   { return '*'; }
"/"                   { return '/'; }
"%"                   { return '%'; }
"="                   { return '='; }
"("                   { return '('; }
")"                   { return ')'; }
","                   { return ','; }
":"                   { return ':'; }

.                     { /* Ignore unknown characters */ }

%%
