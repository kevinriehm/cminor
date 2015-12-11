%code requires {
#include <stdint.h>
#include <stdio.h>

#include "arg.h"
#include "cminor.h"
#include "decl.h"
#include "expr.h"
#include "scan.h"
#include "stmt.h"
#include "str.h"
#include "type.h"
#include "util.h"
}

%code provides {
decl_t *parse_ast;

void parse(FILE *);
}

%{
// For linked lists in values
#define APPEND(_list, _node) do { \
	if((_list).tail) \
		(_list).tail->next = (_node); \
	else (_list).head = (_node); \
\
	(_list).tail = (_node); \
} while(0)

void yyerror(const char *);
%}

/* Makes the error messages sent to yyerror *much* more useful */
%error-verbose

%union {
	char c;
	int64_t i;
	str_t s;

	arg_t *arg;
	decl_t *decl;
	expr_t *expr;
	stmt_t *stmt;
	type_t *type;

	// Permits efficient left recursion in the grammar
	struct { arg_t *head, *tail; } args;
	struct { decl_t *head, *tail; } decls;
	struct { expr_t *head, *tail; } exprs;
	struct { stmt_t *head, *tail; } stmts;
}

%type <arg> arg
%type <decl> decl decl_func decl_var
%type <expr> expr expr0 expr1 expr2 expr3 expr4 expr5 expr6 expr7 expr8
%type <expr> optional_expr array
%type <stmt> stmt stmt_decl stmt_non_decl stmt_non_decl_block
%type <stmt> stmt_non_decl_matched stmt_non_decl_other
%type <stmt> stmt_non_decl_matched_block
%type <type> atomic_type func_type arg_type var_type

%type <args> args args_nonempty
%type <decls> decls
%type <exprs> exprs exprs_nonempty arrays_nonempty
%type <stmts> stmts

%token TOKEN_LPAREN TOKEN_RPAREN
%token TOKEN_LBRACE TOKEN_RBRACE
%token TOKEN_LBRACKET TOKEN_RBRACKET

%token TOKEN_PERCENT TOKEN_ASTERISK TOKEN_PLUS TOKEN_MINUS
%token TOKEN_NOT TOKEN_SLASH TOKEN_CARET TOKEN_EQUAL

%token TOKEN_COMMA TOKEN_COLON TOKEN_SEMICOLON

%token TOKEN_DECREMENT TOKEN_INCREMENT

%token TOKEN_EQ TOKEN_NE TOKEN_LT TOKEN_LE TOKEN_GT TOKEN_GE

%token TOKEN_AND TOKEN_OR

%token TOKEN_ARRAY TOKEN_BOOLEAN TOKEN_CHAR TOKEN_ELSE TOKEN_FALSE
%token TOKEN_FOR TOKEN_FUNCTION TOKEN_IF TOKEN_INTEGER TOKEN_PRINT
%token TOKEN_RETURN TOKEN_STRING TOKEN_TRUE TOKEN_VOID TOKEN_WHILE

%token <s> TOKEN_IDENTIFIER

%token <i> TOKEN_INTEGER_LITERAL
%token <c> TOKEN_CHARACTER_LITERAL
%token <s> TOKEN_STRING_LITERAL

%%

root: decls {
	parse_ast = $1.head;

	if(cminor_mode == CMINOR_PARSE)
		decl_print(parse_ast,0);
    }
    ;

decls: { $$.head = $$.tail = NULL; }
     | decls decl { APPEND($1,$2); $$ = $1; }
     ;

decl: decl_func { $$ = $1; }
    | decl_var { $$ = $1; }
    ;

decl_func: TOKEN_IDENTIFIER TOKEN_COLON func_type TOKEN_SEMICOLON {
	$$ = decl_create($1,$3,NULL,NULL);
         }
         | TOKEN_IDENTIFIER TOKEN_COLON func_type TOKEN_EQUAL TOKEN_LBRACE
           stmts TOKEN_RBRACE {
	$$ = decl_create($1,$3,NULL,
		stmt_create(STMT_BLOCK,NULL,NULL,NULL,NULL,$6.head,NULL));
         }
         ;

decl_var: TOKEN_IDENTIFIER TOKEN_COLON var_type TOKEN_SEMICOLON {
	$$ = decl_create($1,$3,NULL,NULL);
        }
        | TOKEN_IDENTIFIER TOKEN_COLON var_type TOKEN_EQUAL expr
          TOKEN_SEMICOLON { $$ = decl_create($1,$3,$5,NULL); }
        | TOKEN_IDENTIFIER TOKEN_COLON var_type TOKEN_EQUAL array
          TOKEN_SEMICOLON { $$ = decl_create($1,$3,$5,NULL); }
        ;

atomic_type: TOKEN_BOOLEAN {
	$$ = type_create(TYPE_BOOLEAN,0,NULL,NULL,false);
           }
           | TOKEN_CHAR { $$ = type_create(TYPE_CHARACTER,0,NULL,NULL,false); }
           | TOKEN_INTEGER {
	$$ = type_create(TYPE_INTEGER,0,NULL,NULL,false);
           }
           | TOKEN_STRING { $$ = type_create(TYPE_STRING,0,NULL,NULL,false); }
           ;

func_type: TOKEN_FUNCTION atomic_type args {
	$$ = type_create(TYPE_FUNCTION,0,$3.head,$2,false);
         }
         | TOKEN_FUNCTION TOKEN_VOID args {
	$$ = type_create(TYPE_FUNCTION,0,$3.head,type_create(
		TYPE_VOID,0,NULL,NULL,false),false);
         }
         ;

arg_type: atomic_type { $$ = $1; }
        | TOKEN_ARRAY TOKEN_LBRACKET TOKEN_RBRACKET var_type {
	$$ = type_create(TYPE_ARRAY,0,NULL,$4,false);
        }
        ;

var_type: atomic_type { $$ = $1; }
        | TOKEN_ARRAY TOKEN_LBRACKET TOKEN_INTEGER_LITERAL TOKEN_RBRACKET
          var_type {
	$$ = type_create(TYPE_ARRAY,$3,NULL,$5,false);
        }
        ;

exprs: { $$.head = $$.tail = NULL; }
     | exprs_nonempty { $$ = $1; }
     ;

exprs_nonempty: expr { $$.head = $$.tail = $1; }
              | exprs_nonempty TOKEN_COMMA expr { APPEND($1,$3); $$ = $1; }
              ;

expr: expr1 TOKEN_EQUAL expr { $$ = expr_create(EXPR_ASSIGN,$1,$3); }
    | expr8 { $$ = $1; }
    ;

expr8: expr8 TOKEN_OR expr7 { $$ = expr_create(EXPR_OR,$1,$3); }
     | expr7 { $$ = $1; }

expr7: expr7 TOKEN_AND expr6 { $$ = expr_create(EXPR_AND,$1,$3); }
     | expr6 { $$ = $1; }
     ;

expr6: expr6 TOKEN_EQ expr5 { $$ = expr_create(EXPR_EQ,$1,$3); }
     | expr6 TOKEN_NE expr5 { $$ = expr_create(EXPR_NE,$1,$3); }
     | expr6 TOKEN_LT expr5 { $$ = expr_create(EXPR_LT,$1,$3); }
     | expr6 TOKEN_LE expr5 { $$ = expr_create(EXPR_LE,$1,$3); }
     | expr6 TOKEN_GT expr5 { $$ = expr_create(EXPR_GT,$1,$3); }
     | expr6 TOKEN_GE expr5 { $$ = expr_create(EXPR_GE,$1,$3); }
     | expr5 { $$ = $1; }
     ;

expr5: expr5 TOKEN_PLUS expr4 { $$ = expr_create(EXPR_ADD,$1,$3); }
     | expr5 TOKEN_MINUS expr4 { $$ = expr_create(EXPR_SUBTRACT,$1,$3); }
     | expr4 { $$ = $1; }
     ;

expr4: expr4 TOKEN_ASTERISK expr3 { $$ = expr_create(EXPR_MULTIPLY,$1,$3); }
     | expr4 TOKEN_SLASH expr3 { $$ = expr_create(EXPR_DIVIDE,$1,$3); }
     | expr4 TOKEN_PERCENT expr3 { $$ = expr_create(EXPR_REMAINDER,$1,$3); }
     | expr3 { $$ = $1; }
     ;

expr3: expr3 TOKEN_CARET expr2 { $$ = expr_create(EXPR_EXPONENT,$1,$3); }
     | expr2 { $$ = $1; }
     ;

expr2: TOKEN_MINUS expr2 { $$ = expr_create(EXPR_NEGATE,$2,NULL); }
     | TOKEN_NOT expr2 { $$ = expr_create(EXPR_NOT,$2,NULL); }
     | expr1 { $$ = $1; }
     ;

expr1: expr1 TOKEN_INCREMENT { $$ = expr_create(EXPR_INCREMENT,$1,NULL); }
     | expr1 TOKEN_DECREMENT { $$ = expr_create(EXPR_DECREMENT,$1,NULL); }
     | expr1 TOKEN_LBRACKET expr TOKEN_RBRACKET {
	$$ = expr_create(EXPR_SUBSCRIPT,$1,$3);
     }
     | expr0 { $$ = $1; }
     ;

expr0: TOKEN_TRUE { $$ = expr_create_boolean(true); }
     | TOKEN_FALSE { $$ = expr_create_boolean(false); }
     | TOKEN_CHARACTER_LITERAL { $$ = expr_create_character($1); }
     | TOKEN_INTEGER_LITERAL { $$ = expr_create_integer($1); }
     | TOKEN_STRING_LITERAL { $$ = expr_create_string($1); }
     | TOKEN_IDENTIFIER { $$ = expr_create_reference($1); }
     | TOKEN_IDENTIFIER TOKEN_LPAREN exprs TOKEN_RPAREN {
	$$ = expr_create(EXPR_CALL,expr_create_reference($1),$3.head);
     }
     | TOKEN_LPAREN expr TOKEN_RPAREN { $$ = $2; }
     ;

optional_expr: { $$ = NULL; }
             | expr { $$ = $1; }
             ;

      ;

arrays_nonempty: array { $$.head = $$.tail = $1; }
               | arrays_nonempty TOKEN_COMMA array { APPEND($1,$3); $$ = $1; }
               ;

array: TOKEN_LBRACE exprs TOKEN_RBRACE {
	$$ = expr_create(EXPR_ARRAY,$2.head,NULL);
     }
     | TOKEN_LBRACE arrays_nonempty TOKEN_RBRACE {
	$$ = expr_create(EXPR_ARRAY,$2.head,NULL);
     }
     ;

args: TOKEN_LPAREN TOKEN_RPAREN { $$.head = $$.tail = NULL; }
    | TOKEN_LPAREN args_nonempty TOKEN_RPAREN { $$ = $2; }
    ;

args_nonempty: arg { $$.head = $$.tail = $1; }
             | args_nonempty TOKEN_COMMA arg { APPEND($1,$3); $$ = $1; }
             ;

arg: TOKEN_IDENTIFIER TOKEN_COLON arg_type { $$ = arg_create($1,$3); }
   ;

stmts: { $$.head = $$.tail = NULL; }
     | stmts stmt { APPEND($1,$2); $$ = $1; }
     ;

stmt: stmt_decl { $$ = $1; }
    | stmt_non_decl { $$ = $1; }
    ;

stmt_decl: decl_var {
	$$ = stmt_create(STMT_DECL,$1,NULL,NULL,NULL,NULL,NULL);
       }
       ;

stmt_non_decl: stmt_non_decl_other { $$ = $1; }
       | TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN stmt_non_decl_block {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,NULL);
       }
       | TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN stmt_non_decl_matched_block
         TOKEN_ELSE stmt_non_decl_block {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,$7);
       }
       | TOKEN_FOR TOKEN_LPAREN optional_expr TOKEN_SEMICOLON optional_expr
         TOKEN_SEMICOLON optional_expr TOKEN_RPAREN stmt_non_decl_block {
	$$ = stmt_create(STMT_FOR,NULL,$3,$5,$7,$9,NULL);
       }
       ;

stmt_non_decl_matched: stmt_non_decl_other { $$ = $1; }
       | TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN stmt_non_decl_matched_block
         TOKEN_ELSE stmt_non_decl_matched_block {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,$7);
       }
       | TOKEN_FOR TOKEN_LPAREN optional_expr TOKEN_SEMICOLON optional_expr
         TOKEN_SEMICOLON optional_expr TOKEN_RPAREN stmt_non_decl_matched_block
         {
	$$ = stmt_create(STMT_FOR,NULL,$3,$5,$7,$9,NULL);
       }
       ;

stmt_non_decl_other: expr TOKEN_SEMICOLON {
	$$ = stmt_create(STMT_EXPR,NULL,NULL,$1,NULL,NULL,NULL);
       }
       | TOKEN_PRINT exprs TOKEN_SEMICOLON {
	$$ = stmt_create(STMT_PRINT,NULL,NULL,$2.head,NULL,NULL,NULL);
       }
       | TOKEN_RETURN TOKEN_SEMICOLON {
	$$ = stmt_create(STMT_RETURN,NULL,NULL,NULL,NULL,NULL,NULL);
       }
       | TOKEN_RETURN expr TOKEN_SEMICOLON {
	$$ = stmt_create(STMT_RETURN,NULL,NULL,$2,NULL,NULL,NULL);
       }
       | TOKEN_LBRACE stmts TOKEN_RBRACE {
	$$ = stmt_create(STMT_BLOCK,NULL,NULL,NULL,NULL,$2.head,NULL);
       }
       ;

stmt_non_decl_block: stmt_non_decl {
	// This just makes pretty printing and scoping a bit simpler
	$$ = $1->op == STMT_BLOCK ? $1
		: stmt_create(STMT_BLOCK,NULL,NULL,NULL,NULL,$1,NULL);
       }
       ;

stmt_non_decl_matched_block: stmt_non_decl_matched {
	$$ = $1->op == STMT_BLOCK ? $1
		: stmt_create(STMT_BLOCK,NULL,NULL,NULL,NULL,$1,NULL);
       }
       ;

%%

void yyerror(const char *msg) {
	parse_die("line %i: %s",currentline,msg);
}

void parse(FILE *f) {
	yyin = f;
	currentline = 1;
	yyparse();
}

