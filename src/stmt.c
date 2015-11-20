#include <stdio.h>

#include "expr.h"
#include "decl.h"
#include "scope.h"
#include "stmt.h"
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

