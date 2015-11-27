#include "codegen.h"
#include "decl.h"
#include "expr.h"

#include "gen/parse.tab.h"

void codegen(FILE *f) {
	decl_codegen(parse_ast,f);
	expr_print_asm_strings(f);
}

