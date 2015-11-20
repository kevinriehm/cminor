#include <inttypes.h>
#include <stdio.h>

#include "arg.h"
#include "cminor.h"
#include "expr.h"
#include "scope.h"
#include "symbol.h"
#include "type.h"
#include "pp_util.h"
#include "util.h"

static char *operators[] = {
	[EXPR_ADD]       = "+",
	[EXPR_AND]       = "&&",
	[EXPR_ASSIGN]    = "=",
	[EXPR_DECREMENT] = "--",
	[EXPR_DIVIDE]    = "/",
	[EXPR_EXPONENT]  = "^",
	[EXPR_INCREMENT] = "++",
	[EXPR_MULTIPLY]  = "*",
	[EXPR_NEGATE]    = "-",
	[EXPR_NOT]       = "!",
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

// Special print function used when type errors are found
// Format: type (expression)
void expr_type_print(expr_t *this) {
	expr_t *next = this->next;
	this->next = NULL;

	type_print(this->type);
	printf(" (");
	expr_print(this);
	putchar(')');

	this->next = next;
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

void expr_typecheck(expr_t *this) {
	arg_t *arg;
	size_t m, n;
	expr_t *expr;
	bool constant, fail, manyargs;

	while(this) {
		expr_typecheck(this->left);
		expr_typecheck(this->right);

		fail = false;

		switch(this->op) {
		case EXPR_ADD:
		case EXPR_DIVIDE:
		case EXPR_EXPONENT:
		case EXPR_MULTIPLY:
		case EXPR_REMAINDER:
		case EXPR_SUBTRACT:
			fail = !type_is(this->left->type,TYPE_INTEGER)
				|| !type_is(this->right->type,TYPE_INTEGER);
			this->type = type_create(TYPE_INTEGER,0,NULL,NULL,
				this->left->type->constant
					&& this->right->type->constant);
			break;

		case EXPR_AND:
		case EXPR_OR:
			fail = !type_is(this->left->type,TYPE_BOOLEAN)
				|| !type_is(this->right->type,TYPE_BOOLEAN);
			this->type = type_create(TYPE_BOOLEAN,0,NULL,NULL,
				this->left->type->constant
					&& this->right->type->constant);
			break;

		case EXPR_ASSIGN:
			if(!type_eq(this->left->type,this->right->type)) {
				cminor_errorcount++;
				printf("type error: cannot assign to ");
				expr_type_print(this->left);
				printf(" from ");
				expr_type_print(this->right);
				putchar('\n');
			}

			if(type_is(this->left->type,TYPE_ARRAY)
				|| type_is(this->left->type,TYPE_FUNCTION)) {
				cminor_errorcount++;
				printf("type_error: cannot assign to ");
				expr_type_print(this->left);
				putchar('\n');
			}

			this->type = this->left->type;
			break;

		case EXPR_CALL:
			if(!type_is(this->left->type,TYPE_FUNCTION)) {
				cminor_errorcount++;
				printf("type error: cannot invoke ");
				expr_type_print(this->left);
				printf(" as a function\n");
				break;
			}

			for(arg = this->left->type->args, expr = this->right,
				n = 0; arg && expr;
				arg = arg->next, expr = expr->next, n++) {
				if(!type_eq(arg->type,expr->type)) {
					cminor_errorcount++;
					printf("type error: argument %zu to ",
						n);
					expr_print(this->left);
					printf(" is ");
					expr_type_print(expr);
					printf(" but should be ");
					type_print(arg->type);
					putchar('\n');
				}
			}

			if(arg || expr) {
				manyargs = !!arg;

				for(m = n; arg || expr;
					arg = arg ? arg->next : NULL,
					expr = expr ? expr->next : NULL, m++);

				cminor_errorcount++;
				printf("type error: call to ");
				expr_print(this->left);
				printf(" has %zu argument%s but should have "
					"%zu\n",
					manyargs ? n : m,
					(manyargs ? n : m) == 1 ? "" : "s",
					manyargs ? m : n);
			}

			this->type = this->left->type->subtype;
			break;

		case EXPR_DECREMENT:
		case EXPR_INCREMENT:
			if(!this->left->type->lvalue) {
				cminor_errorcount++;
				printf("type error: cannot apply the operator "
					"'%s' to a non-lvalue (",
					operators[this->op]);
				expr_print(this->left);
				printf(")\n");
			}

			fail = !type_is(this->left->type,TYPE_INTEGER);
			this->type = type_create(TYPE_INTEGER,0,NULL,NULL,
				this->left->type->constant);
			this->type->lvalue = this->left->type->lvalue;
			break;

		case EXPR_NEGATE:
			fail = !type_is(this->left->type,TYPE_INTEGER);
			this->type = type_create(TYPE_INTEGER,0,NULL,NULL,
				this->left->type->constant);
			break;

		case EXPR_NOT:
			fail = !type_is(this->left->type,TYPE_BOOLEAN);
			this->type = type_create(TYPE_BOOLEAN,
				0,NULL,NULL,this->left->type->constant);
			break;

		case EXPR_SUBSCRIPT:
			if(!type_is(this->left->type,TYPE_ARRAY)) {
				cminor_errorcount++;
				printf("type error: cannot index into ");
				expr_type_print(this->left);
				putchar('\n');
			}

			if(!type_is(this->right->type,TYPE_INTEGER)) {
				cminor_errorcount++;
				printf("type error: array index is ");
				expr_type_print(this->right);
				printf(" but should be integer\n");
			}

			if(this->type = this->left->type->subtype, !this->type)
				this->type = type_create(
					TYPE_VOID,0,NULL,NULL,false);
			this->type->constant = this->left->type->constant;
			break;

		case EXPR_EQ:
		case EXPR_NE:
			fail = !type_eq(this->left->type,this->right->type)
				|| type_is(this->left->type,TYPE_ARRAY)
				|| type_is(this->left->type,TYPE_FUNCTION)
				|| type_is(this->right->type,TYPE_ARRAY)
				|| type_is(this->right->type,TYPE_FUNCTION);
			this->type = type_create(TYPE_BOOLEAN,0,NULL,NULL,
				this->left->type->constant
					&& this->right->type->constant);
			break;

		case EXPR_GE:
		case EXPR_GT:
		case EXPR_LE:
		case EXPR_LT:
			fail = !type_is(this->left->type,TYPE_INTEGER)
				|| !type_is(this->right->type,TYPE_INTEGER);
			this->type = type_create(TYPE_BOOLEAN,0,NULL,NULL,
				this->left->type->constant
					&& this->right->type->constant);
			break;

		case EXPR_ARRAY:
			for(expr = this->left, constant = true, n = 0; expr;
				expr = expr->next, n++) {
				constant &= expr->type->constant;

				if(!type_eq(this->left->type,expr->type)) {
					cminor_errorcount++;
					printf("type error: element %zu of "
						"array intializer is ",n);
					expr_type_print(expr);
					printf(" but first element of the "
						"array is ");
					expr_type_print(this->left);
					printf(", which is inconsistent\n");
				}
			}

			this->type = type_create(
				TYPE_ARRAY,n,NULL,this->left->type,constant);
			break;

		case EXPR_BOOLEAN:
			this->type
				= type_create(TYPE_BOOLEAN,0,NULL,NULL,true);
			break;

		case EXPR_CHARACTER:
			this->type
				= type_create(TYPE_CHARACTER,0,NULL,NULL,true);
			break;

		case EXPR_INTEGER:
			this->type
				= type_create(TYPE_INTEGER,0,NULL,NULL,true);
			break;

		case EXPR_REFERENCE:
			this->type = type_clone(this->symbol->type);
			this->type->lvalue = true;
			break;

		case EXPR_STRING:
			this->type = type_create(TYPE_STRING,0,NULL,NULL,true);
			break;
		}

		if(fail) {
			cminor_errorcount++;

			printf("type error: cannot apply the operator '%s' "
				"to ",operators[this->op]);
			expr_type_print(this->left);

			if(this->right) {
				printf(" and ");
				expr_type_print(this->right);
			}

			putchar('\n');
		}

		this = this->next;
	}
}

