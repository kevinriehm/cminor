#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdbool.h>

#include "decl.h"
#include "htable.h"
#include "str.h"

typedef enum {
	SYMBOL_ARG,
	SYMBOL_GLOBAL,
	SYMBOL_LOCAL
} symbol_level_t;

typedef struct symbol {
	str_t name;
	struct type *type;

	symbol_level_t level;
	bool prototype;
	decl_t *func; // Enclosing function

	size_t index;
} symbol_t;

typedef_htable_t(symbol_t);

symbol_t *symbol_create(str_t, struct type *, symbol_level_t, bool,
	struct decl *);

void symbol_print(symbol_t *);

#endif

