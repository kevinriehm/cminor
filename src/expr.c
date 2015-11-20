#include <inttypes.h>
#include <stdio.h>

#include "cminor.h"
#include "expr.h"
#include "scope.h"
#include "symbol.h"
#include "pp_util.h"
#include "util.h"

expr_t *expr_create(expr_op_t op, expr_t *left, expr_t *right) {
	expr_t *this = new(expr_t,{
		.op = op,
		.left = left,
		.right = right,
		.parent = NULL,
		.next = NULL
	});

	if(left)
		left->parent = this;
	if(right)
		right->parent = this;

	return this;
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

void expr_print(expr_t *this) {
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

		// Expressions that should never need parentheses
		// around them or their children
		[EXPR_ARRAY]     = -1,
		[EXPR_CALL]      = -1,
		[EXPR_SUBSCRIPT] = -1,

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

	if(!this)
		return;

	needparen = this->parent
		&& precedence[this->op] > 0
		&& precedence[this->parent->op] > 0
		&& precedence[this->op] > precedence[this->parent->op]
		|| this->op == EXPR_NEGATE; // Avoid x - -y => x--y

	while(this) {
		if(needparen)
			putchar('(');

		switch(this->op) {
		case EXPR_ARRAY:
			putchar('{');
			expr_print(this->left);
			putchar('}');
			break;

		case EXPR_CALL:
			expr_print(this->left);
			putchar('(');
			expr_print(this->right);
			putchar(')');
			break;

		case EXPR_DECREMENT:
			expr_print(this->left);
			printf("--");
			break;

		case EXPR_INCREMENT:
			expr_print(this->left);
			printf("++");
			break;

		case EXPR_NEGATE:
			putchar('-');
			expr_print(this->left);
			break;

		case EXPR_NOT:
			putchar('!');
			expr_print(this->left);
			break;

		case EXPR_SUBSCRIPT:
			expr_print(this->left);
			putchar('[');
			expr_print(this->right);
			putchar(']');
			break;

		case EXPR_SUBTRACT:
			expr_print(this->left);
			printf("%s%s",operators[this->op],
				this->right->op == EXPR_NEGATE ? " " : "");
			expr_print(this->right);
			break;

		case EXPR_BOOLEAN:
			printf(this->b ? "true" : "false");
			break;

		case EXPR_CHARACTER:
			printf("'%s'",this->c == '\n' ? "\\n"
				: this->c == '\0' ? "\\0"
				: this->c == '\'' ? "\\'"
				: (char []) {this->c, '\0'});
			break;

		case EXPR_INTEGER:
			printf("%"PRIi64,this->i);
			break;

		case EXPR_REFERENCE:
			printf("%s",this->s.v);
			break;

		case EXPR_STRING:
			putchar('"');
			for(size_t i = 0; i < this->s.n; i++)
				printf("%s",this->s.v[i] == '\n' ? "\\n"
					: this->s.v[i] == '\0' ? "\\0"
					: this->s.v[i] == '"' ? "\\\""
					: (char []) {this->s.v[i], '\0'});
			putchar('"');
			break;

		// Binary operators
		default:
			expr_print(this->left);
			printf("%s",operators[this->op]);
			expr_print(this->right);
			break;
		}

		if(needparen)
			putchar(')');

		if(this = this->next)
			putchar(',');
	}
}

void expr_resolve(expr_t *this) {
	while(this) {
		if(this->op == EXPR_REFERENCE) {
			if(this->symbol = scope_lookup(this->s)) {
				if(cminor_mode == MODE_RESOLVE) {
					printf("%s resolves to ",this->s.v);
					symbol_print(this->symbol);
					putchar('\n');
				}
			} else resolve_error("%s is not defined",this->s.v);
		}

		expr_resolve(this->left);
		expr_resolve(this->right);

		this = this->next;
	}
}

