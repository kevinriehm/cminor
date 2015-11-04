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
	while(type) {
		switch(type->type) {
		case TYPE_ARRAY:
			printf("array [%"PRIi64"]",type->size);
			break;

		case TYPE_BOOLEAN:
			printf("boolean");
			break;

		case TYPE_CHARACTER:
			printf("character");
			break;

		case TYPE_FUNCTION:
			printf("function");
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

		if(type = type->subtype)
			putchar(' ');
	}
}

