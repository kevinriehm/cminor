#include "htable.h"
#include "pp_util.h"
#include "scope.h"
#include "str.h"
#include "symbol.h"
#include "util.h"

typedef struct scope {
	struct scope *parent;
	struct scope *function;
	htable_t(symbol_t) table;

	size_t nargs;
	size_t nglobals;
	size_t nlocals; // Local count of this and parent scopes
	size_t nfunclocals; // Total function local count
} scope_t;

static scope_t *scope = NULL;

void scope_enter(bool function) {
	scope = new(scope_t,{
		.parent = scope,
		.table = htable_new(symbol_t)
	});

	// Give us access to the function local count
	if(function)
		scope->function = scope;
	else if(scope->parent)
		scope->function = scope->parent->function;

	// Give us access to the parent local count (we can reuse the local
	// indices used by our siblings, so we start our locals at the parent's
	// current count)
	if(scope->parent)
		scope->nlocals = scope->parent->nlocals;
}

void scope_leave() {
	if(!scope)
		die("tried to leave undefined scope");

	scope = scope->parent;
}

bool scope_is_global() {
	return scope && !scope->parent;
}

size_t scope_num_function_locals() {
	return scope->function ? scope->function->nfunclocals : 0;
}

void scope_bind(str_t name, symbol_t *s) {
	if(!scope)
		die("tried to bind in undefined scope");

	switch(s->level) {
	case SYMBOL_ARG:    s->index = scope->nargs++;    break;
	case SYMBOL_GLOBAL: s->index = scope->nglobals++; break;

	// Sibling scopes within a function can overlap their locals, as long
	// as the function's total local count is kept at the maximum of any of
	// its descendants
	case SYMBOL_LOCAL:
		s->index = scope->nlocals++;

		if(scope->nlocals > scope->function->nfunclocals)
			scope->function->nfunclocals = scope->nlocals;
		break;
	}

	if(htable_lookup(scope->table,name))
		resolve_error("%s is redefined",name.v);
	else htable_insert(scope->table,name,s);
}

symbol_t *scope_lookup(str_t name) {
	symbol_t *symbol;

	if(!scope)
		die("tried to lookup in undefined scope");

	for(scope_t *s = scope; s; s = s->parent)
		if(symbol = htable_lookup(s->table,name))
			return symbol;

	return NULL;
}

symbol_t *scope_lookup_local(str_t name) {
	if(!scope)
		die("tried to lookup in undefined scope");

	return htable_lookup(scope->table,name);
}

