#include <inttypes.h>
#include <stdio.h>

#include "expr.h"
#include "pp_util.h"

expr_t *expr_create(expr_op_t op, expr_t *left, expr_t *right) {
	expr_t *expr = new(expr_t,{
		.op = op,
		.left = left,
		.right = right,
		.parent = NULL,
		.next = NULL
	});

	if(left)
		left->parent = expr;
	if(right)
		right->parent = expr;

	return expr;
}

expr_t *expr_create_boolean(bool val) {
	return new(expr_t,{
		.op = EXPR_BOOLEAN,
		.b = val,
		.parent = NULL,
		.next = NULL
	});
}

expr_t *expr_create_character(char val) {
	return new(expr_t,{
		.op = EXPR_CHARACTER,
		.c = val,
		.parent = NULL,
		.next = NULL
	});
}

expr_t *expr_create_integer(int64_t val) {
	return new(expr_t,{
		.op = EXPR_INTEGER,
		.i = val,
		.parent = NULL,
		.next = NULL
	});
}

expr_t *expr_create_reference(str_t val) {
	return new(expr_t,{
		.op = EXPR_REFERENCE,
		.s = val,
		.parent = NULL,
		.next = NULL
	});
}

expr_t *expr_create_string(str_t val) {
	return new(expr_t,{
		.op = EXPR_STRING,
		.s = val,
		.parent = NULL,
		.next = NULL
	});
}

void expr_print(expr_t *expr) {
	// For parenthesis insertion
	static int precedence[] = {
		[EXPR_DECREMENT] = 0,
		[EXPR_INCREMENT] = 0,

		[EXPR_NEGATE] = 1,
		[EXPR_NOT]    = 1,

		[EXPR_EXPONENT] = 2,

		[EXPR_DIVIDE]    = 3,
		[EXPR_MULTIPLY]  = 3,
		[EXPR_REMAINDER] = 3,

		[EXPR_ADD]      = 4,
		[EXPR_SUBTRACT] = 4,

		[EXPR_EQ] = 5,
		[EXPR_GE] = 5,
		[EXPR_GT] = 5,
		[EXPR_LE] = 5,
		[EXPR_LT] = 5,
		[EXPR_NE] = 5,

		[EXPR_AND] = 6,

		[EXPR_OR] = 7,

		[EXPR_ASSIGN] = 8,

		[EXPR_ARRAY] = 9,

		[EXPR_BOOLEAN]   = -1,
		[EXPR_CHARACTER] = -1,
		[EXPR_INTEGER]   = -1,
		[EXPR_REFERENCE] = -1,
		[EXPR_STRING]    = -1
	};

	// Binary operators
	static char *operators[] = {
		[EXPR_ADD]       = "+",
		[EXPR_AND]       = "&&",
		[EXPR_ASSIGN]    = "=",
		[EXPR_DIVIDE]    = "/",
		[EXPR_EXPONENT]  = "^",
		[EXPR_MULTIPLY]  = "*",
		[EXPR_OR]        = "||",
		[EXPR_REMAINDER] = "%",
		[EXPR_SUBTRACT]  = "-",

		[EXPR_EQ] = "==",
		[EXPR_GE] = ">=",
		[EXPR_GT] = ">",
		[EXPR_LE] = "<=",
		[EXPR_LT] = "<",
		[EXPR_NE] = "!="
	};

	bool needparen;

	needparen = expr->parent
		&& precedence[expr->op] > precedence[expr->parent->op];

	while(expr) {
		if(needparen)
			putchar('(');

		switch(expr->op) {
		case EXPR_ARRAY:
			putchar('{');
			expr_print(expr->left);
			putchar('}');
			break;

		case EXPR_CALL:
			expr_print(expr->left);
			putchar('(');
			expr_print(expr->right);
			putchar(')');
			break;

		case EXPR_DECREMENT:
			expr_print(expr->left);
			printf("--");
			break;

		case EXPR_INCREMENT:
			expr_print(expr->left);
			printf("++");
			break;

		case EXPR_NEGATE:
			putchar('-');
			expr_print(expr->left);
			break;

		case EXPR_NOT:
			putchar('!');
			expr_print(expr->left);
			break;

		case EXPR_SUBSCRIPT:
			expr_print(expr->left);
			putchar('[');
			expr_print(expr->right);
			putchar(']');
			break;

		case EXPR_SUBTRACT:
			expr_print(expr->left);
			printf("%s%s",operators[expr->op],
				expr->right->op == EXPR_NEGATE ? " " : "");
			expr_print(expr->right);
			break;

		case EXPR_BOOLEAN:
			printf(expr->b ? "true" : "false");
			break;

		case EXPR_CHARACTER:
			printf("'%s'",expr->c == '\n' ? "\\n"
				: expr->c == '\0' ? "\\0"
				: expr->c == '\'' ? "\\'"
				: (char []) {expr->c, '\0'});
			break;

		case EXPR_INTEGER:
			printf("%"PRIi64,expr->i);
			break;

		case EXPR_REFERENCE:
			printf("%s",expr->s.v);
			break;

		case EXPR_STRING:
			putchar('"');
			for(size_t i = 0; i < expr->s.n; i++)
				printf("%s",expr->s.v[i] == '\n' ? "\\n"
					: expr->s.v[i] == '\0' ? "\\0"
					: expr->s.v[i] == '"' ? "\\\""
					: (char []) {expr->s.v[i], '\0'});
			putchar('"');
			break;

		// Binary operators
		default:
			expr_print(expr->left);
			printf("%s",operators[expr->op]);
			expr_print(expr->right);
			break;
		}

		if(needparen)
			putchar(')');

		if(expr = expr->next)
			printf(", ");
	}
}

