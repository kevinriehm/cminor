%code requires {
#include <stdint.h>
#include <stdio.h>

#include "arg.h"
#include "decl.h"
#include "expr.h"
#include "scan.h"
#include "stmt.h"
#include "str.h"
#include "type.h"
#include "util.h"
}

%code provides {
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

void yyerror(char *);
%}

/* Allows us to omit the prefix in the grammar, for conciseness */
%define api.token.prefix {TOKEN_}

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
%type <expr> expr expr0 expr1 expr2 expr3 expr4 expr5 expr6 expr7 array lvalue
%type <stmt> stmt stmt_matched stmt_other
%type <type> type

%type <args> args args_nonempty
%type <decls> decls
%type <exprs> exprs exprs_nonempty arrays_nonempty
%type <stmts> stmts

%token LPAREN RPAREN
%token LBRACE RBRACE
%token LBRACKET RBRACKET

%token PERCENT ASTERISK PLUS MINUS
%token NOT SLASH CARET EQUAL

%token COMMA COLON SEMICOLON

%token DECREMENT INCREMENT

%token EQ NE LT LE GT GE

%token AND OR

%token ARRAY BOOLEAN CHAR ELSE FALSE
%token FOR FUNCTION IF INTEGER PRINT
%token RETURN STRING TRUE VOID WHILE

%token <s> IDENTIFIER

%token <i> INTEGER_LITERAL
%token <c> CHARACTER_LITERAL
%token <s> STRING_LITERAL

%%

root: decls { decl_print($1.head,0); }
    ;

decls: { $$.head = $$.tail = NULL; }
     | decls decl { APPEND($1,$2); $$ = $1; }
     ;

decl: decl_func { $$ = $1; }
    | decl_var { $$ = $1; }
    ;

decl_func: IDENTIFIER COLON FUNCTION type args SEMICOLON {
	$$ = decl_create(
		$1,type_create(TYPE_FUNCTION,0,$5.head,$4),NULL,NULL,NULL);
         }
         | IDENTIFIER COLON FUNCTION type args EQUAL LBRACE stmts RBRACE {
	$$ = decl_create($1,
		type_create(TYPE_FUNCTION,0,$5.head,$4),$5.head,NULL,$8.head);
         }
         ;

decl_var: IDENTIFIER COLON type SEMICOLON {
	$$ = decl_create($1,$3,NULL,NULL,NULL);
        }
        | IDENTIFIER COLON type EQUAL expr SEMICOLON {
	$$ = decl_create($1,$3,NULL,$5,NULL);
        }
        | IDENTIFIER COLON type EQUAL array SEMICOLON {
	$$ = decl_create($1,$3,NULL,$5,NULL);
        }
        ;

type: BOOLEAN { $$ = type_create(TYPE_BOOLEAN,0,NULL,NULL); }
    | CHAR { $$ = type_create(TYPE_CHARACTER,0,NULL,NULL); }
    | INTEGER { $$ = type_create(TYPE_INTEGER,0,NULL,NULL); }
    | STRING { $$ = type_create(TYPE_STRING,0,NULL,NULL); }
    | VOID { $$ = type_create(TYPE_VOID,0,NULL,NULL); }
    | ARRAY LBRACKET RBRACKET type { $$ = type_create(TYPE_ARRAY,0,NULL,$4); }
    | ARRAY LBRACKET INTEGER_LITERAL RBRACKET type {
	$$ = type_create(TYPE_ARRAY,$3,NULL,$5);
    }
    ;

exprs: { $$.head = $$.tail = NULL; }
     | exprs_nonempty { $$ = $1; }
     ;

exprs_nonempty: expr { $$.head = $$.tail = $1; }
              | exprs_nonempty COMMA expr { APPEND($1,$3); $$ = $1; }
              ;

expr: lvalue EQUAL expr { $$ = expr_create(EXPR_ASSIGN,$1,$3); }
    | expr7 { $$ = $1; }
    ;

expr7: expr7 OR expr6 { $$ = expr_create(EXPR_OR,$1,$3); }
     | expr6 { $$ = $1; }

expr6: expr6 AND expr5 { $$ = expr_create(EXPR_AND,$1,$3); }
     | expr5 { $$ = $1; }
     ;

expr5: expr5 EQ expr4 { $$ = expr_create(EXPR_EQ,$1,$3); }
     | expr5 NE expr4 { $$ = expr_create(EXPR_NE,$1,$3); }
     | expr5 LT expr4 { $$ = expr_create(EXPR_LT,$1,$3); }
     | expr5 LE expr4 { $$ = expr_create(EXPR_LE,$1,$3); }
     | expr5 GT expr4 { $$ = expr_create(EXPR_GT,$1,$3); }
     | expr5 GE expr4 { $$ = expr_create(EXPR_GE,$1,$3); }
     | expr4 { $$ = $1; }
     ;

expr4: expr4 PLUS expr3 { $$ = expr_create(EXPR_ADD,$1,$3); }
     | expr4 MINUS expr3 { $$ = expr_create(EXPR_SUBTRACT,$1,$3); }
     | expr3 { $$ = $1; }
     ;

expr3: expr3 ASTERISK expr2 { $$ = expr_create(EXPR_MULTIPLY,$1,$3); }
     | expr3 SLASH expr2 { $$ = expr_create(EXPR_DIVIDE,$1,$3); }
     | expr3 PERCENT expr2 { $$ = expr_create(EXPR_REMAINDER,$1,$3); }
     | expr2 { $$ = $1; }
     ;

expr2: expr2 CARET expr1 { $$ = expr_create(EXPR_EXPONENT,$1,$3); }
     | expr1 { $$ = $1; }
     ;

expr1: MINUS expr1 { $$ = expr_create(EXPR_NEGATE,$2,NULL); }
     | NOT expr1 { $$ = expr_create(EXPR_NOT,$2,NULL); }
     | expr0 { $$ = $1; }
     ;

expr0: TRUE { $$ = expr_create_boolean(true); }
     | FALSE { $$ = expr_create_boolean(false); }
     | CHARACTER_LITERAL { $$ = expr_create_character($1); }
     | INTEGER_LITERAL { $$ = expr_create_integer($1); }
     | STRING_LITERAL { $$ = expr_create_string($1); }
     | lvalue { $$ = $1; }
     | lvalue INCREMENT { $$ = expr_create(EXPR_INCREMENT,$1,NULL); }
     | lvalue DECREMENT { $$ = expr_create(EXPR_DECREMENT,$1,NULL); }
     | IDENTIFIER LPAREN exprs RPAREN {
	$$ = expr_create(EXPR_CALL,expr_create_reference($1),$3.head);
     }
     | LPAREN expr RPAREN { $$ = $2; }
     ;

lvalue: IDENTIFIER { $$ = expr_create_reference($1); }
      | IDENTIFIER LBRACKET expr RBRACKET {
	$$ = expr_create(EXPR_SUBSCRIPT,expr_create_reference($1),$3);
      }
      ;

arrays_nonempty: array { $$.head = $$.tail = $1; }
               | arrays_nonempty COMMA array { APPEND($1,$3); $$ = $1; }
               ;

array: LBRACE exprs RBRACE { $$ = expr_create(EXPR_ARRAY,$2.head,NULL); }
     | LBRACE arrays_nonempty RBRACE {
	$$ = expr_create(EXPR_ARRAY,$2.head,NULL);
     }
     ;

args: LPAREN RPAREN { $$.head = $$.tail = NULL; }
    | LPAREN args_nonempty RPAREN { $$ = $2; }
    ;

args_nonempty: arg { $$.head = $$.tail = $1; }
             | args_nonempty COMMA arg { APPEND($1,$3); $$ = $1; }
             ;

arg: IDENTIFIER COLON type { $$ = arg_create($1,$3); }
   ;

stmts: { $$.head = $$.tail = NULL; }
     | stmts stmt { APPEND($1,$2); $$ = $1; }
     ;

stmt: stmt_other { $$ = $1; }
    | IF LPAREN expr RPAREN stmt {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,NULL);
    }
    | IF LPAREN expr RPAREN stmt_matched ELSE stmt {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,$7);
    }
    | FOR LPAREN expr SEMICOLON expr SEMICOLON expr RPAREN stmt {
	$$ = stmt_create(STMT_FOR,NULL,$3,$5,$7,$9,NULL);
    }
    ;

stmt_matched: stmt_other { $$ = $1; }
            | IF LPAREN expr RPAREN stmt_matched ELSE stmt_matched {
	$$ = stmt_create(STMT_IF_ELSE,NULL,NULL,$3,NULL,$5,$7);
            }
            | FOR LPAREN expr SEMICOLON expr SEMICOLON expr RPAREN
              stmt_matched {
	$$ = stmt_create(STMT_FOR,NULL,$3,$5,$7,$9,NULL);
            }
            ;

stmt_other: decl_var {
	$$ = stmt_create(STMT_DECL,$1,NULL,NULL,NULL,NULL,NULL);
          }
          | expr SEMICOLON {
	$$ = stmt_create(STMT_EXPR,NULL,NULL,$1,NULL,NULL,NULL);
          }
          | PRINT exprs SEMICOLON {
	$$ = stmt_create(STMT_PRINT,NULL,NULL,$2.head,NULL,NULL,NULL);
          }
          | RETURN SEMICOLON {
	$$ = stmt_create(STMT_RETURN,NULL,NULL,NULL,NULL,NULL,NULL);
          }
          | RETURN expr SEMICOLON {
	$$ = stmt_create(STMT_RETURN,NULL,NULL,$2,NULL,NULL,NULL);
          }
          | LBRACE stmts RBRACE { $$ = $2.head; }
          ;

%%

void yyerror(char *msg) {
	parse_die("line %i: %s",currentline,msg);
}

void parse(FILE *f) {
	yyin = f;
	currentline = 1;
	yyparse();
}

