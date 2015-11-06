#include <inttypes.h>
#include <stdio.h>

#include "arg.h"
#include "pp_util.h"
#include "type.h"

type_t *type_create(type_type_t type, int64_t size, arg_t *args,
	type_t *subtype) {
	return new(type_t,{
		.type = type,
		.size = size,
		.args = args,
		.subtype = subtype
	});
}

void type_print(type_t *type) {
	if(!type)
		return;

	switch(type->type) {
	case TYPE_ARRAY:
		if(type->size)
			printf("array [%"PRIi64"] ",type->size);
		else printf("array [] ");
		type_print(type->subtype);
		break;

	case TYPE_BOOLEAN:
		printf("boolean");
		break;

	case TYPE_CHARACTER:
		printf("character");
		break;

	case TYPE_FUNCTION:
		printf("function ");
		type_print(type->subtype);
		putchar(' ');
		arg_print(type->args);
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

