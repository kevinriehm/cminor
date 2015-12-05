#ifndef ARG_H
#define ARG_H

#include <stdbool.h>

#include "str.h"

typedef struct arg {
	str_t name;
	struct type *type;

	struct symbol *symbol;

	struct arg *next;
} arg_t;

arg_t *arg_create(str_t, struct type *);

size_t arg_count(arg_t *);
bool arg_eq(arg_t *, arg_t *);

void arg_print(arg_t *);

#endif

