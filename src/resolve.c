#include "decl.h"
#include "scope.h"

#include "gen/parse.tab.h"

void resolve() {
	scope_enter(NULL);

	decl_resolve(parse_ast);

	scope_leave();
}

