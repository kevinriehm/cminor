#include <stdio.h>

#include "decl.h"
#include "pp_util.h"
#include "symbol.h"
#include "type.h"

symbol_t *symbol_create(str_t name, type_t *type, symbol_level_t level,
	bool prototype, decl_t *func) {
	return new(symbol_t,{
		.name = name,
		.type = type,
		.level = level,
		.prototype = prototype,
		.func = func
	});
}

void symbol_print(symbol_t *this) {
	switch(this->level) {
	case SYMBOL_ARG:    printf("argument %zu",this->index); break;
	case SYMBOL_GLOBAL: printf("global %s",this->name.v);   break;
	case SYMBOL_LOCAL:  printf("local %zu",this->index);    break;
	}
}

