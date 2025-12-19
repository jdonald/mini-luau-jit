%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex();
extern int yylineno;
void yyerror(const char* s);

BlockNode* programRoot = nullptr;
%}

%union {
    long long ival;
    bool bval;
    char* sval;
    ASTNode* node;
    BlockNode* block;
    std::vector<std::unique_ptr<ASTNode>>* args;
    std::vector<std::string>* params;
}

%token <ival> INTEGER
%token <bval> BOOLEAN
%token <sval> STRING IDENTIFIER
%token FUNCTION END IF THEN ELSE ELSEIF WHILE DO RETURN LOCAL TYPE PRINT
%token EQ NE LT LE GT GE AND OR NOT

%type <node> statement expression primary_expr unary_expr multiplicative_expr
%type <node> additive_expr comparison_expr logical_and_expr logical_or_expr
%type <node> function_def if_stmt while_stmt assignment return_stmt function_call
%type <block> program statement_list block
%type <args> arg_list arg_list_items
%type <params> param_list param_list_items
%type <sval> type_annotation opt_type_annotation

%left OR
%left AND
%left EQ NE
%left LT LE GT GE
%left '+' '-'
%left '*' '/' '%'
%right NOT
%right UNARY

%%

program:
    statement_list { programRoot = $1; $$ = $1; }
    ;

statement_list:
    /* empty */ { $$ = new BlockNode(); }
    | statement_list statement {
        $$ = $1;
        if ($2) $$->addStatement($2);
    }
    ;

statement:
    assignment { $$ = $1; }
    | function_def { $$ = $1; }
    | function_call { $$ = $1; }
    | if_stmt { $$ = $1; }
    | while_stmt { $$ = $1; }
    | return_stmt { $$ = $1; }
    | PRINT '(' arg_list ')' {
        PrintNode* pn = new PrintNode();
        if ($3) {
            pn->args = std::move(*$3);
            delete $3;
        }
        $$ = pn;
    }
    ;

assignment:
    LOCAL IDENTIFIER opt_type_annotation '=' expression {
        $$ = new AssignmentNode($2, $5, $3 ? $3 : "");
        free($2);
        if ($3) free($3);
    }
    | IDENTIFIER '=' expression {
        $$ = new AssignmentNode($1, $3);
        free($1);
    }
    ;

opt_type_annotation:
    /* empty */ { $$ = nullptr; }
    | ':' type_annotation { $$ = $2; }
    ;

type_annotation:
    IDENTIFIER { $$ = $1; }
    ;

function_def:
    FUNCTION IDENTIFIER '(' param_list ')' opt_type_annotation block END {
        $$ = new FunctionDefNode($2, *$4, $7, {}, $6 ? $6 : "");
        free($2);
        if ($6) free($6);
        delete $4;
    }
    ;

param_list:
    /* empty */ { $$ = new std::vector<std::string>(); }
    | param_list_items { $$ = $1; }
    ;

param_list_items:
    IDENTIFIER opt_type_annotation {
        $$ = new std::vector<std::string>();
        $$->push_back($1);
        free($1);
        if ($2) free($2);
    }
    | param_list_items ',' IDENTIFIER opt_type_annotation {
        $$ = $1;
        $$->push_back($3);
        free($3);
        if ($4) free($4);
    }
    ;

function_call:
    IDENTIFIER '(' arg_list ')' {
        FunctionCallNode* fc = new FunctionCallNode($1);
        if ($3) {
            fc->args = std::move(*$3);
            delete $3;
        }
        $$ = fc;
        free($1);
    }
    ;

arg_list:
    /* empty */ { $$ = nullptr; }
    | arg_list_items { $$ = $1; }
    ;

arg_list_items:
    expression {
        $$ = new std::vector<std::unique_ptr<ASTNode>>();
        $$->push_back(std::unique_ptr<ASTNode>($1));
    }
    | arg_list_items ',' expression {
        $$ = $1;
        $$->push_back(std::unique_ptr<ASTNode>($3));
    }
    ;

if_stmt:
    IF expression THEN block END {
        $$ = new IfNode($2, $4);
    }
    | IF expression THEN block ELSE block END {
        $$ = new IfNode($2, $4, $6);
    }
    ;

while_stmt:
    WHILE expression DO block END {
        $$ = new WhileNode($2, $4);
    }
    ;

block:
    statement_list { $$ = $1; }
    ;

return_stmt:
    RETURN expression {
        $$ = new ReturnNode($2);
    }
    | RETURN {
        $$ = new ReturnNode(nullptr);
    }
    ;

expression:
    logical_or_expr { $$ = $1; }
    ;

logical_or_expr:
    logical_and_expr { $$ = $1; }
    | logical_or_expr OR logical_and_expr {
        $$ = new BinaryOpNode(BinaryOpType::OR, $1, $3);
    }
    ;

logical_and_expr:
    comparison_expr { $$ = $1; }
    | logical_and_expr AND comparison_expr {
        $$ = new BinaryOpNode(BinaryOpType::AND, $1, $3);
    }
    ;

comparison_expr:
    additive_expr { $$ = $1; }
    | comparison_expr EQ additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::EQ, $1, $3);
    }
    | comparison_expr NE additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::NE, $1, $3);
    }
    | comparison_expr LT additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::LT, $1, $3);
    }
    | comparison_expr LE additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::LE, $1, $3);
    }
    | comparison_expr GT additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::GT, $1, $3);
    }
    | comparison_expr GE additive_expr {
        $$ = new BinaryOpNode(BinaryOpType::GE, $1, $3);
    }
    ;

additive_expr:
    multiplicative_expr { $$ = $1; }
    | additive_expr '+' multiplicative_expr {
        $$ = new BinaryOpNode(BinaryOpType::ADD, $1, $3);
    }
    | additive_expr '-' multiplicative_expr {
        $$ = new BinaryOpNode(BinaryOpType::SUB, $1, $3);
    }
    ;

multiplicative_expr:
    unary_expr { $$ = $1; }
    | multiplicative_expr '*' unary_expr {
        $$ = new BinaryOpNode(BinaryOpType::MUL, $1, $3);
    }
    | multiplicative_expr '/' unary_expr {
        $$ = new BinaryOpNode(BinaryOpType::DIV, $1, $3);
    }
    | multiplicative_expr '%' unary_expr {
        $$ = new BinaryOpNode(BinaryOpType::MOD, $1, $3);
    }
    ;

unary_expr:
    primary_expr { $$ = $1; }
    | NOT unary_expr {
        $$ = new UnaryOpNode(UnaryOpType::NOT, $2);
    }
    | '-' unary_expr %prec UNARY {
        $$ = new UnaryOpNode(UnaryOpType::NEG, $2);
    }
    ;

primary_expr:
    INTEGER { $$ = new IntegerNode($1); }
    | BOOLEAN { $$ = new BooleanNode($1); }
    | STRING { $$ = new StringNode($1); free($1); }
    | IDENTIFIER { $$ = new VariableNode($1); free($1); }
    | function_call { $$ = $1; }
    | '(' expression ')' { $$ = $2; }
    ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "Parse error at line %d: %s\n", yylineno, s);
}
