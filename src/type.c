#include <inttypes.h>
#include <stdio.h>

#include "arg.h"
#include "pp_util.h"
#include "type.h"

type_t *type_create(type_type_t type, int64_t size, arg_t *args,
	type_t *subtype, bool constant) {
	return new(type_t,{
		.type = type,
		.size = size,
		.args = args,
		.subtype = subtype,
		.constant = constant
	});
}

type_t *type_clone(type_t *this) {
	return this ? new(type_t,{
		.type = this->type,
		.size = this->size,
		.args = this->args,
		.subtype = type_clone(this->subtype),
		.constant = this->constant,
		.lvalue = this->lvalue
	}) : NULL;
}

bool type_eq(type_t *a, type_t *b) {
	return a == b // Shortcut; also two NULLs are equal
		|| a && b // NULL check
			&& a->type == b->type
			&& (!a->size || !b->size || a->size == b->size)
			&& type_eq(a->subtype,b->subtype)
			&& arg_eq(a->args,b->args);
}

bool type_is(type_t *this, type_type_t type) {
	return this && this->type == type;
}

void type_print(type_t *this) {
	if(!this)
		return;

	switch(this->type) {
	case TYPE_ARRAY:
		if(this->size)
			printf("array [%"PRIi64"] ",this->size);
		else printf("array [] ");
		type_print(this->subtype);
		break;

	case TYPE_BOOLEAN:
		printf("boolean");
		break;

	case TYPE_CHARACTER:
		printf("character");
		break;

	case TYPE_FUNCTION:
		printf("function ");
		type_print(this->subtype);
		putchar(' ');
		arg_print(this->args);
		break;

	case TYPE_INTEGER:
		printf("integer");
		break;

	case TYPE_STRING:
		printf("string");
		break;

	case TYPE_VOID:
		printf("void");
		break;
	}
}

