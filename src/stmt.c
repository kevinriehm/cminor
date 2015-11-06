#include <stdio.h>

#include "expr.h"
#include "decl.h"
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

void stmt_print(stmt_t *stmt, int indent) {
	char indentstr[indent + 1];

	for(int i = 0; i < indent; i++)
		indentstr[i] = '\t';
	indentstr[indent] = '\0';

	while(stmt) {
		switch(stmt->op) {
		case STMT_BLOCK:
			printf("{\n");
			stmt_print(stmt->body,indent);
			printf("%.*s}\n",indent - 1,indentstr);
			break;

		case STMT_DECL:
			decl_print(stmt->decl,indent);
			break;

		case STMT_EXPR:
			printf("%s",indentstr);
			expr_print(stmt->expr);
			printf(";\n");
			break;

		case STMT_FOR:
			printf("%sfor(",indentstr);
			expr_print(stmt->init_expr);
			putchar(';');
			expr_print(stmt->expr);
			putchar(';');
			expr_print(stmt->next_expr);
			printf(") ");
			stmt_print(stmt->body,indent + 1);
			break;

		case STMT_IF_ELSE:
			printf("%sif(",indentstr);
			expr_print(stmt->expr);
			printf(") ");
			stmt_print(stmt->body,indent + 1);
			if(stmt->else_body) {
				printf("%selse ",indentstr);
				stmt_print(stmt->else_body,indent + 1);
			}
			break;

		case STMT_PRINT:
			printf("%sprint%s",indentstr,stmt->expr ? " " : "");
			expr_print(stmt->expr);
			printf(";\n");
			break;

		case STMT_RETURN:
			printf("%sreturn%s",indentstr,stmt->expr ? " " : "");
			if(stmt->expr)
				expr_print(stmt->expr);
			printf(";\n");
			break;
		}

		stmt = stmt->next;
	}
}

