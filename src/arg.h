#ifndef ARG_H
#define ARG_H

#include "str.h"

typedef struct arg {
	str_t name;
	struct type *type;

	struct arg *next;
} arg_t;

arg_t *arg_create(str_t, struct type *);

void arg_print(arg_t *);

#endif

