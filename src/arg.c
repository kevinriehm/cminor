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

void arg_print(arg_t *arg) {
	while(arg) {
		printf("%s: ",arg->name.v);

		type_print(arg->type);

		if(arg = arg->next)
			printf(", ");
	}
}

