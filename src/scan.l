%{
#include <stdio.h>

#include "str.h"
#include "token.h"
#include "util.h"

// For conciseness
#define TOKEN(_name) do { \
	tokenname = #_name; \
	return TOKEN_##_name; \
} while(0)

static char parse_escaped_char(char *);

static union {
	char c;
	str_t s;
} tokenvalue;

static int currentline;
static char *tokenname;
%}

%x COMMENT STRING

alpha [A-Za-z]
digit [0-9]
echar ([^\"']|\\.)

%%

\n     currentline++;
[ \t]+ ;

"//".*\n         ;
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
"/" TOKEN(SLASH);
"^" TOKEN(CARET);
=   TOKEN(EQUALS);

, TOKEN(COMMA);
: TOKEN(COLON);
; TOKEN(SEMICOLON);

--   TOKEN(DECREMENT);
"++" TOKEN(INCREMENT);

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
	TOKEN(IDENTIFIER);
}

{digit}+ TOKEN(INTEGER_LITERAL);

'({echar}|\")' {
	tokenvalue.c = parse_escaped_char(yytext + 1);
	TOKEN(CHARACTER_LITERAL);
}

\"                 {
	BEGIN STRING;
	tokenvalue.s = str_new("",0);
}
<STRING>[^"\n]|\\. {
	if(tokenvalue.s.n >= 255)
		scan_die("string literals cannot be more than 256 characters "
			"(including the terminating null byte)");
	str_append_c(tokenvalue.s,parse_escaped_char(yytext));
}
<STRING>\n         scan_die("string literals cannot be more than one line");
<STRING><<EOF>>    scan_die("unterminated string");
<STRING>\"         {
	BEGIN INITIAL;
	TOKEN(STRING_LITERAL);
}

. scan_die("%c is not a valid character",yytext[0]); // Catch-all

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
			printf(" %c",tokenvalue.c);

		if(tok == TOKEN_STRING_LITERAL)
			printf(" %s",tokenvalue.s.v);

		putchar('\n');
	}
}

