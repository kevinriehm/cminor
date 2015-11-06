%{
#include <stdio.h>

#include "scan.h"
#include "str.h"
#include "util.h"

#include "gen/parse.tab.h"

// For conciseness
#define TOKEN(_name) do { \
	tokenname = #_name; \
	return TOKEN_##_name; \
} while(0)

static char *tokenname;

static char parse_escaped_char(char *);
%}

%x COMMENT STRING

alpha [A-Za-z]
digit [0-9]
echar ([^\\"']|\\.)

%%

\n     currentline++;
[ \t]+ ;

"//".*\n         currentline++;
"/*"             BEGIN COMMENT;
<COMMENT>.       ;
<COMMENT>\n      currentline++;
<COMMENT><<EOF>> scan_die("unterminated comment");
<COMMENT>"*/"    BEGIN INITIAL;

"(" TOKEN(LPAREN);
")" TOKEN(RPAREN);
"{" TOKEN(LBRACE);
"}" TOKEN(RBRACE);
"[" TOKEN(LBRACKET);
]   TOKEN(RBRACKET);

%   TOKEN(PERCENT);
"*" TOKEN(ASTERISK);
"+" TOKEN(PLUS);
-   TOKEN(MINUS);
!   TOKEN(NOT);
"/" TOKEN(SLASH);
"^" TOKEN(CARET);
=   TOKEN(EQUAL);

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
function TOKEN(FUNCTION);
if       TOKEN(IF);
integer  TOKEN(INTEGER);
print    TOKEN(PRINT);
return   TOKEN(RETURN);
string   TOKEN(STRING);
true     TOKEN(TRUE);
void     TOKEN(VOID);
while    TOKEN(WHILE);

(_|{alpha})(_|{alpha}|{digit})* {
	if(yyleng > 256)
		scan_die("identifiers cannot be more than 256 characters");
	yylval.s = str_new(yytext,yyleng);
	TOKEN(IDENTIFIER);
}

{digit}+ {
	yylval.i = atoll(yytext);
	TOKEN(INTEGER_LITERAL);
}

'({echar}|\")' {
	yylval.c = parse_escaped_char(yytext + 1);
	TOKEN(CHARACTER_LITERAL);
}

\"                 {
	BEGIN STRING;
	yylval.s = str_new("",0);
}
<STRING>[^"\n]|\\. {
	if(yylval.s.n >= 255)
		scan_die("string literals cannot be more than 256 characters "
			"(including the terminating null byte)");
	str_append_c(yylval.s,parse_escaped_char(yytext));
}
<STRING>\n         scan_die("string literals cannot be more than one line");
<STRING><<EOF>>    scan_die("unterminated string");
<STRING>\"         {
	BEGIN INITIAL;
	TOKEN(STRING_LITERAL);
}

. scan_die("unexpected character %c",yytext[0]); // Catch-all

%%

int yywrap() {
	return 1;
}

static char parse_escaped_char(char *p) {
	if(p[0] == '\\') {
		switch(p[1]) {
		case '0': return '\0';
		case 'n': return '\n';
		default:  return p[1];
		}
	} else return p[0];
}

void scan(FILE *f) {
	int tok;

	yyin = f;
	currentline = 1;

	while(tok = yylex()) {
		printf("%s",tokenname);

		if(tok == TOKEN_CHARACTER_LITERAL)
			printf(" %c",yylval.c);

		if(tok == TOKEN_STRING_LITERAL)
			printf(" %s",yylval.s.v);

		putchar('\n');
	}
}

