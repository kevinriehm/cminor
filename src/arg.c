#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg.h"
#include "pp_util.h"
#include "type.h"

arg_t *arg_create(str_t name, type_t *type) {
	return new(arg_t,{
		.name = name,
		.type = type,
		.next = NULL
	});
}

size_t arg_count(arg_t *this) {
	size_t argi;

	for(argi = 0; this; this = this->next, argi++);

	return argi;
}

bool arg_eq(arg_t *a, arg_t *b) {
	while(a && b) {
		if(!type_eq(a->type,b->type))
			return false;

		a = a->next;
		b = b->next;
	}

	return !a && !b;
}

void arg_print(arg_t *this) {
	putchar('(');

	while(this) {
		printf(" %s: ",this->name.v);

		type_print(this->type);

		if(this = this->next)
			printf(", ");
	}

	printf(" )");
}

