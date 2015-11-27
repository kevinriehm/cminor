#include <stdio.h>

#include "arg.h"
#include "cminor.h"
#include "decl.h"
#include "expr.h"
#include "pp_util.h"
#include "reg.h"
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

void decl_codegen(decl_t *this, FILE *f) {
	arg_t *arg;
	int argi, reg;

	while(this) {
		if(type_is(this->type,TYPE_FUNCTION)) {
			fputs(".text\n",f);
			fprintf(f,".global %s\n",this->name.v);
			fprintf(f,"%s:\n",this->name.v);

			fputs("push %rbp\n",f);
			fputs("mov %rsp, %rbp\n",f);

			for(arg = this->type->args, argi = 0;
				arg; arg = arg->next, argi++)
				fprintf(f,"push %%%s\n",reg_name_arg(argi));

			fprintf(f,"sub $%zu, %%rsp\n",8*this->type->nlocals);

			fputs("push %rbx\n",f);
			fputs("push %r12\n",f);
			fputs("push %r13\n",f);
			fputs("push %r14\n",f);
			fputs("push %r15\n",f);

			stmt_codegen(this->body,f);
		} else if(this->symbol->level == SYMBOL_GLOBAL) {
			fprintf(f,".data\n.globl %s\n%s: ",this->name.v,
				this->name.v);

			if(this->value)
				expr_print_asm(expr_eval_constant(this->value),
					f,true);
			else fprintf(f,".space %zu\n",8*type_size(this->type));

			fputc('\n',f);
		} else if(this->value) {
			reg = expr_codegen(this->value,f,false);
			fprintf(f,"mov %%%s, -%zu(%%rbp)\n",reg_name(reg),
				8*(this->type->nargs + this->symbol->index));
			reg_free(reg);
		}

		this = this->next;
	}
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
		if(this->symbol = scope_lookup_local(this->name)) {
			if(this->symbol->prototype
				&& type_eq(this->type,this->symbol->type))
				this->symbol->prototype
					= this->symbol->prototype
						&& !!this->body;
			else resolve_error("%s is redefined",this->name.v);
		} else {
			this->symbol = symbol_create(this->name,this->type,
				scope_is_global()
					? SYMBOL_GLOBAL : SYMBOL_LOCAL,
				type_is(this->type,TYPE_FUNCTION)
					&& !this->body);

			scope_bind(this->name,this->symbol);
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

			this->type->nlocals = scope_num_function_locals();

			scope_leave();
		}

		this = this->next;
	}
}

void decl_typecheck(decl_t *this) {
	while(this) {
		// Variable initialization
		if(this->value) {
			expr_typecheck(this->value);

			if(!type_eq(this->type,this->value->type)) {
				cminor_errorcount++;
				printf("type error: cannot initialize ");
				type_print(this->type);
				printf(" (%s) with ",this->name.v);
				type_print(this->value->type);
				printf(" (");
				expr_print(this->value);
				printf(")\n");
			} else if(this->symbol->level == SYMBOL_GLOBAL
				&& !this->value->type->constant) {
				cminor_errorcount++;
				printf("type error: global variable (%s) "
					"cannot be initialized with "
					"non-constant expression (",
					this->name.v);
				expr_print(this->value);
				printf(")\n");
			}
		}

		// Function declaration
		if(this->body)
			stmt_typecheck(this->body,this);

		this = this->next;
	}
}

