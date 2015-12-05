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
	int argi, *regs;
	reg_real_t *realregs;

	while(this) {
		if(type_is(this->type,TYPE_FUNCTION) && this->body) {
			reg_reset();

			fputs(".text\n",f);
			fprintf(f,".globl %s\n",this->name.v);
			fprintf(f,"%s:\n",this->name.v);

			fputs("push %rbp\n",f);
			fputs("mov %rsp, %rbp\n",f);

			fprintf(f,"sub $%s$spill, %%rsp\n",this->name.v);

			// Assign the arguments to virtual registers
			realregs = (reg_real_t []) {
				REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8,
				REG_R9
			};

			for(arg = this->type->args, argi = 0;
				arg; arg = arg->next, argi++)
				arg->symbol->reg = argi < 6
					? reg_assign_real(realregs[argi])
					: reg_assign_local(5 - argi);

			// Preserve the callee-saved registers
			regs = (int []) {
				reg_assign_real(REG_RBX),
				reg_assign_real(REG_R12),
				reg_assign_real(REG_R13),
				reg_assign_real(REG_R14),
				reg_assign_real(REG_R15)
			};

			stmt_codegen(this->body,f);
			fputs("99:\n",f);

			// Restore the callee-saved registers
			reg_map_v(5,regs,(reg_real_t []) {
				REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15
			},f);

			fputs("mov %rbp, %rsp\n",f);
			fputs("pop %rbp\n",f);
			fputs("ret\n",f);

			fprintf(f,".set %s$spill, %zu\n",
				this->name.v,8*reg_frame_size());
		} else if(!type_is(this->type,TYPE_FUNCTION)
			&& this->symbol->level == SYMBOL_GLOBAL) {
			fprintf(f,".data\n.globl %s\n%s: ",this->name.v,
				this->name.v);

			if(this->value)
				expr_print_asm(expr_eval_constant(this->value),
					f,true);
			else fprintf(f,".space %zu\n",8*type_size(this->type));

			fputc('\n',f);
		} else if(this->value) { // Local variable
			this->symbol->reg = expr_codegen(this->value,f,false,
				this->symbol->func->type->nargs
					+ this->symbol->index
					+ type_size(this->value->type));
			reg_make_persistent(this->symbol->reg);
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
					&& !this->body,
				scope_function());

			scope_bind(this->name,this->symbol);
		}

		// Resolve variable initialization
		if(this->value)
			expr_resolve(this->value);

		// Resolve function declaration
		if(this->body) {
			scope_enter(this);

			for(arg_t *arg = this->type->args; arg;
				arg = arg->next) {
				arg->symbol = symbol_create(
					arg->name,arg->type,SYMBOL_ARG,false,
					scope_function());
				scope_bind(arg->name,arg->symbol);
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

