#include <stdio.h>

#include "arg.h"
#include "cminor.h"
#include "decl.h"
#include "expr.h"
#include "pp_util.h"
#include "scope.h"
#include "stmt.h"
#include "symbol.h"
#include "type.h"
#include "util.h"

decl_t *decl_create(str_t name, type_t *type, expr_t *value, stmt_t *body) {
	return new(decl_t,{
		.name = name,
		.type = type,
		.value = value,
		.body = body,
		.next = NULL
	});
}

void decl_print(decl_t *this, int indent) {
	char indentstr[indent + 1];

	for(int i = 0; i < indent; i++)
		indentstr[i] = '\t';
	indentstr[indent] = '\0';

	while(this) {
		printf("%s%s: ",indentstr,this->name.v);

		type_print(this->type);

		if(this->value) { // Variable
			printf(" = ");
			expr_print(this->value);
			printf(";\n");
		} else if(this->body) { // Function
			printf(" = ");
			stmt_print(this->body,indent + 1);
		} else printf(";\n");

		this = this->next;
	}
}

void decl_resolve(decl_t *this) {
	symbol_t *symbol;

	while(this) {
		// Redundant declarations are not allowed,
		// with the exception of function prototypes
		if(symbol = scope_lookup_local(this->name)) {
			if(symbol->prototype
				&& type_eq(this->type,symbol->type))
				symbol->prototype
					= symbol->prototype && !!this->body;
			else resolve_error("%s is redefined",this->name.v);
		} else {
			symbol = symbol_create(this->name,this->type,
				scope_is_global()
					? SYMBOL_GLOBAL : SYMBOL_LOCAL,
				type_is_function(this->type) && !this->body);

			scope_bind(this->name,symbol);
		}

		// Resolve variable initialization
		if(this->value)
			expr_resolve(this->value);

		// Resolve function declaration
		if(this->body) {
			scope_enter(true);

			for(arg_t *arg = this->type->args; arg;
				arg = arg->next) {
				symbol = symbol_create(
					arg->name,arg->type,SYMBOL_ARG,false);
				scope_bind(arg->name,symbol);
			}

			stmt_resolve(this->body->body);

			scope_leave();
		}

		this = this->next;
	}
}

