#ifndef DECL_H
#define DECL_H

#include "str.h"

typedef struct decl {
	str_t name;
	struct type *type;
	struct expr *value;
	struct stmt *body;

	struct decl *next;
} decl_t;

decl_t *decl_create(str_t, struct type *, struct expr *, struct stmt *);

void decl_print(decl_t *, int);
void decl_resolve(decl_t *);

#endif

