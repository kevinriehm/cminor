#include <stdio.h>

#include "cminor.h"
#include "decl.h"
#include "expr.h"
#include "reg.h"
#include "scope.h"
#include "stmt.h"
#include "type.h"
#include "pp_util.h"

stmt_t *stmt_create(stmt_op_t op, decl_t *decl, expr_t *init_expr,
	expr_t *expr, expr_t *next_expr, stmt_t *body, stmt_t *else_body) {
	return new(stmt_t,{
		.op = op,
		.decl = decl,
		.init_expr = init_expr,
		.expr = expr,
		.next_expr = next_expr,
		.body = body,
		.else_body = else_body,
		.next = NULL
	});
}

void stmt_codegen(stmt_t *this, FILE *f) {
	static size_t nlabels = 0;

	int reg;
	size_t label1, label2;

	while(this) {
		switch(this->op) {
		case STMT_BLOCK:
			stmt_codegen(this->body,f);
			break;

		case STMT_DECL:
			decl_codegen(this->decl,f);
			break;

		case STMT_EXPR:
			reg = expr_codegen(this->expr,f,false,0);
			reg_free(reg);
			break;

		case STMT_FOR:
			label1 = nlabels++;
			label2 = nlabels++;

			reg = expr_codegen(this->init_expr,f,false,0);
			reg_free(reg);

			fprintf(f,".Lstmt_%zu:\n",label1);

			reg = expr_codegen(this->expr,f,false,0);
			fprintf(f,"cmp $0, %s\n",reg_name(reg));
			fprintf(f,"je .Lstmt_%zu\n",label2);
			reg_free(reg);

			stmt_codegen(this->body,f);

			reg = expr_codegen(this->next_expr,f,false,0);
			reg_free(reg);

			fprintf(f,"jmp .Lstmt_%zu\n",label1);
			fprintf(f,".Lstmt_%zu:\n",label2);
			break;

		case STMT_IF_ELSE:
			label1 = nlabels++;
			label2 = nlabels++;

			reg = expr_codegen(this->expr,f,false,0);
			fprintf(f,"cmp $0, %s\n",reg_name(reg));
			fprintf(f,"je .Lstmt_%zu\n",label1);
			reg_free(reg);

			stmt_codegen(this->body,f);

			fprintf(f,"jmp .Lstmt_%zu\n",label2);
			fprintf(f,".Lstmt_%zu:\n",label1);

			stmt_codegen(this->else_body,f);

			fprintf(f,".Lstmt_%zu:\n",label2);
			break;

		case STMT_PRINT:
			for(expr_t *expr = this->expr;
				expr; expr = expr->next) {
				reg = expr_codegen(expr,f,false,0);
				fprintf(f,"mov %s, %%rdi\n",reg_name(reg));
				reg_free(reg);

				fprintf(f,"push %%r10\n");
				fprintf(f,"push %%r11\n");

				fprintf(f,"call print_%s\n",
					  expr->type->type == TYPE_BOOLEAN
					? "boolean"
					: expr->type->type == TYPE_CHARACTER
					? "character"
					: expr->type->type == TYPE_INTEGER
					? "integer"
					: expr->type->type == TYPE_STRING
					? "string" : "<should never happen>");

				fprintf(f,"pop %%r11\n");
				fprintf(f,"pop %%r10\n");
				reg_free(reg);
			}
			break;

		case STMT_RETURN:
			reg = expr_codegen(this->expr,f,false,0);
			fprintf(f,"mov %s, %%rax\n",reg_name(reg));
			reg_free(reg);

			fputs("jmp 99f\n",f);
			break;
		}

		this = this->next;
	}
}

void stmt_print(stmt_t *this, int indent) {
	char indentstr[indent + 1];

	for(int i = 0; i < indent; i++)
		indentstr[i] = '\t';
	indentstr[indent] = '\0';

	while(this) {
		switch(this->op) {
		case STMT_BLOCK:
			printf("{\n");
			stmt_print(this->body,indent);
			printf("%.*s}\n",indent - 1,indentstr);
			break;

		case STMT_DECL:
			decl_print(this->decl,indent);
			break;

		case STMT_EXPR:
			printf("%s",indentstr);
			expr_print(this->expr);
			printf(";\n");
			break;

		case STMT_FOR:
			printf("%sfor(",indentstr);
			expr_print(this->init_expr);
			putchar(';');
			expr_print(this->expr);
			putchar(';');
			expr_print(this->next_expr);
			printf(") ");
			stmt_print(this->body,indent + 1);
			break;

		case STMT_IF_ELSE:
			printf("%sif(",indentstr);
			expr_print(this->expr);
			printf(") ");
			stmt_print(this->body,indent + 1);
			if(this->else_body) {
				printf("%selse ",indentstr);
				stmt_print(this->else_body,indent + 1);
			}
			break;

		case STMT_PRINT:
			printf("%sprint%s",indentstr,this->expr ? " " : "");
			expr_print(this->expr);
			printf(";\n");
			break;

		case STMT_RETURN:
			printf("%sreturn%s",indentstr,this->expr ? " " : "");
			if(this->expr)
				expr_print(this->expr);
			printf(";\n");
			break;
		}

		this = this->next;
	}
}

void stmt_resolve(stmt_t *this) {
	while(this) {
		switch(this->op) {
		case STMT_BLOCK:
			scope_enter(false);
			stmt_resolve(this->body);
			scope_leave();
			break;

		case STMT_DECL:
			decl_resolve(this->decl);
			break;

		case STMT_EXPR:
			expr_resolve(this->expr);
			break;

		case STMT_FOR:
			expr_resolve(this->init_expr);
			expr_resolve(this->expr);
			expr_resolve(this->next_expr);
			stmt_resolve(this->body);
			break;

		case STMT_IF_ELSE:
			expr_resolve(this->expr);
			stmt_resolve(this->body);
			stmt_resolve(this->else_body);
			break;

		case STMT_PRINT:
			expr_resolve(this->expr);
			break;

		case STMT_RETURN:
			expr_resolve(this->expr);
			break;
		}

		this = this->next;
	}
}

void stmt_typecheck(stmt_t *this, decl_t *func) {
	while(this) {
		switch(this->op) {
		case STMT_BLOCK:
			stmt_typecheck(this->body,func);
			break;

		case STMT_DECL:
			decl_typecheck(this->decl);
			break;

		case STMT_EXPR:
			expr_typecheck(this->expr);
			break;

		case STMT_FOR:
			expr_typecheck(this->init_expr);
			expr_typecheck(this->expr);

			if(!type_is(this->expr->type,TYPE_BOOLEAN)) {
				cminor_errorcount++;
				printf("type error: for loop condition is ");
				expr_type_print(this->expr);
				printf(", but must be boolean\n");
			}

			expr_typecheck(this->next_expr);
			stmt_typecheck(this->body,func);
			break;

		case STMT_IF_ELSE:
			expr_typecheck(this->expr);

			if(!type_is(this->expr->type,TYPE_BOOLEAN)) {
				cminor_errorcount++;
				printf("type error: if%s condition is ",
					this->else_body ? "-else" : "");
				expr_type_print(this->expr);
				printf(", but must be boolean\n");
			}

			stmt_typecheck(this->body,func);
			stmt_typecheck(this->else_body,func);
			break;

		case STMT_PRINT:
			expr_typecheck(this->expr);

			for(expr_t *expr = this->expr; expr;
				expr = expr->next) {
				if(type_is(expr->type,TYPE_FUNCTION)
					|| type_is(expr->type,TYPE_VOID)) {
					cminor_errorcount++;
					printf("type error: cannot print ");
					expr_type_print(expr);
					putchar('\n');
				}
			}
			break;

		case STMT_RETURN:
			expr_typecheck(this->expr);

			if(this->expr && !type_eq(
				this->expr->type,func->type->subtype)) {
				cminor_errorcount++;
				printf("type error: cannot return ");
				expr_type_print(this->expr);
				printf(" in a function (%s) that returns ",
					func->name.v);
				type_print(func->type->subtype);
				putchar('\n');
			}

			if(!this->expr
				&& !type_is(func->type->subtype,TYPE_VOID)) {
				cminor_errorcount++;
				printf("type error: return statement has "
					"expression (");
				expr_print(this->expr);
				printf(") in function with void type\n");
			}
			break;
		}

		this = this->next;
	}
}

