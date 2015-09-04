%{
#include "str.h"

int yylex();
void yyerror(const char *);
%}

/* Currently used only for the token definitions */

%union {
	char c;
	str_t s;
}

%token LPAREN RPAREN
%token LBRACE RBRACE
%token LBRACKET RBRACKET

%token PERCENT ASTERISK PLUS MINUS SLASH CARET
%token EQUALS
%token COMMA COLON SEMICOLON

%token INCREMENT DECREMENT

%token EQ NE LT LE GT GE
%token AND OR

%token ARRAY BOOLEAN CHAR ELSE FALSE FOR FOREACH FUNCTION IF INTEGER PRINT
%token RETURN STRING TRUE VOID WHILE

%token IDENTIFIER

%token INTEGER_LITERAL CHARACTER_LITERAL STRING_LITERAL

%%

root: ;

%%

void yyerror(const char *msg) {
	(void) msg;
}

