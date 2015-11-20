#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	TYPE_ARRAY,
	TYPE_BOOLEAN,
	TYPE_CHARACTER,
	TYPE_FUNCTION,
	TYPE_INTEGER,
	TYPE_STRING,
	TYPE_VOID
} type_type_t;

typedef struct type {
	type_type_t type;
	struct type *subtype;

	int64_t size;
	struct arg *args;
} type_t;

type_t *type_create(type_type_t, int64_t, struct arg *, type_t *);

bool type_is_function(type_t *);

bool type_eq(type_t *, type_t *);

void type_print(type_t *);

#endif

