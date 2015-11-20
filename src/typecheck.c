#include "decl.h"

#include "gen/parse.tab.h"

void typecheck() {
	decl_typecheck(parse_ast);
}

