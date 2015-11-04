#include <stdio.h>

#include "arg.h"
#include "decl.h"
#include "expr.h"
#include "pp_util.h"
#include "stmt.h"
#include "type.h"

decl_t *decl_create(str_t name, type_t *type, arg_t *args, expr_t *value,
	stmt_t *body) {
	return new(decl_t,{
		.name = name,
		.type = type,
		.args = args,
		.value = value,
		.body = body,
		.next = NULL
	});
}

void decl_print(decl_t *decl, int indent) {
	char indentstr[indent + 1];

	for(int i = 0; i < indent; i++)
		indentstr[i] = '\t';
	indentstr[indent] = '\0';

	while(decl) {
		printf("%s%s: ",indentstr,decl->name.v);

		type_print(decl->type);

		if(decl->body) {
			printf(" ( ");
			arg_print(decl->args);
			printf(" )");
		}

		if(decl->value) {
			printf(" = ");
			expr_print(decl->value);
			printf(";\n");
		} else if(decl->body) {
			printf(" = {\n");
			stmt_print(decl->body,indent + 1);
			printf("}\n");
		} else printf(";\n");

		decl = decl->next;
	}
}

