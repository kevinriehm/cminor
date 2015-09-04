%{
#include <stdio.h>

#include "str.h"
#include "util.h"

#include "gen/parse.tab.h"

// For conciseness
#define TOKEN(_name) do { \
	tokenname = #_name; \
	return (_name); \
} while(0)

char parse_char(char **);

static int currentline;
static char *tokenname;
%}

%x COMMENT

alpha [A-Za-z]
digit [0-9]
echar ([^\"']|\\.)

%%

\n     currentline++;
[ \t]+ ;

"//"[^\n]\n   ;
"/*"          BEGIN COMMENT;
<COMMENT>.    ;
<COMMENT>\n   currentline++;
<COMMENT>"*/" ;

"(" TOKEN(LPAREN);
")" TOKEN(RPAREN);
{   TOKEN(LBRACE);
}   TOKEN(RBRACE);
"[" TOKEN(LBRACKET);
]   TOKEN(RBRACKET);

%   TOKEN(PERCENT);
"*" TOKEN(ASTERISK);
"+" TOKEN(PLUS);
-   TOKEN(MINUS); // Must be distinct from integer literals
"/" TOKEN(SLASH);
"^" TOKEN(CARET);

= TOKEN(EQUALS);

, TOKEN(COMMA);
: TOKEN(COLON);
; TOKEN(SEMICOLON);

"++" TOKEN(INCREMENT);
--   TOKEN(DECREMENT);

==   TOKEN(EQ);
!=   TOKEN(NE);
"<"  TOKEN(LT);
"<=" TOKEN(LE);
>    TOKEN(GT);
>=   TOKEN(GE);

&&   TOKEN(AND);
"||" TOKEN(OR);

array    TOKEN(ARRAY);
boolean  TOKEN(BOOLEAN);
char     TOKEN(CHAR);
else     TOKEN(ELSE);
false    TOKEN(FALSE);
for      TOKEN(FOR);
foreach  TOKEN(FOREACH);
function TOKEN(FUNCTION);
if       TOKEN(IF);
integer  TOKEN(INTEGER);
print    TOKEN(PRINT);
return   TOKEN(RETURN);
string   TOKEN(STRING);
true     TOKEN(TRUE);
void     TOKEN(VOID);
while    TOKEN(WHILE);

(_|{alpha})(_|{alpha}|{digit})* TOKEN(IDENTIFIER);

(0|[1-9]{digit}*) TOKEN(INTEGER_LITERAL);

'({echar}|\")' {
	char *p = yytext + 1;
	yylval.c = parse_char(&p);
	TOKEN(CHARACTER_LITERAL);
}

\"({echar}|')*\" {
	yylval.s = str_new("",0);
	for(char *p = yytext + 1; p < yytext + yyleng - 1;)
		str_append_c(yylval.s,parse_char(&p));
	TOKEN(STRING_LITERAL);
}

. scan_die("%c is not a valid character",yytext[0]); // Catch-all

%%

int yywrap() {
	return 1;
}

char parse_char(char **p) {
	char c;

	if(c = *(*p)++, c == '\\') {
		switch(c = *(*p)++) {
		case '0': return '\0';
		case 'n': return '\n';
		default:  return c;
		}
	} else return c;
}

void tokenize(FILE *f) {
	int tok;

	yyin = f;
	currentline = 1;

	while(tok = yylex()) {
		printf("%s",tokenname);

		if(tok == CHARACTER_LITERAL)
			printf(" %c",yylval.c);

		if(tok == STRING_LITERAL)
			printf(" %s",yylval.s.v);

		putchar('\n');
	}
}

