#include "decl.h"
#include "htable.h"
#include "pp_util.h"
#include "scope.h"
#include "str.h"
#include "symbol.h"
#include "type.h"
#include "util.h"

typedef struct scope {
	struct scope *parent;
	struct scope *funcscope;
	htable_t(symbol_t) table;

	size_t nargs;
	size_t nglobals;
	size_t nlocals; // Local count of this and parent scopes
	size_t nfunclocals; // Total function local count

	decl_t *func;
} scope_t;

static scope_t *scope = NULL;

void scope_enter(decl_t *func) {
	scope = new(scope_t,{
		.parent = scope,
		.table = htable_new(symbol_t),
		.func = func
	});

	// Give us access to the function local count
	if(func)
		scope->funcscope = scope;
	else if(scope->parent)
		scope->funcscope = scope->parent->funcscope;

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

decl_t *scope_function() {
	return scope && scope->funcscope ? scope->funcscope->func : NULL;
}

size_t scope_num_function_locals() {
	return scope->funcscope ? scope->funcscope->nfunclocals : 0;
}

void scope_bind(str_t name, symbol_t *symbol) {
	if(!scope)
		die("tried to bind in undefined scope");

	switch(symbol->level) {
	case SYMBOL_ARG:    symbol->index = scope->nargs++;    break;
	case SYMBOL_GLOBAL: symbol->index = scope->nglobals++; break;

	// Sibling scopes within a function can overlap their locals, as long
	// as the function's total local count is kept at the maximum of any of
	// its descendants.
	case SYMBOL_LOCAL:
		symbol->index = scope->nlocals;
		scope->nlocals += type_size(symbol->type);

		if(scope->nlocals > scope->funcscope->nfunclocals)
			scope->funcscope->nfunclocals = scope->nlocals;
		break;
	}

	if(htable_lookup(scope->table,name))
		resolve_error("%s is redefined",name.v);
	else htable_insert(scope->table,name,symbol);
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

