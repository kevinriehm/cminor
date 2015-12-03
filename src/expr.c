#include <inttypes.h>
#include <stdio.h>

#include "arg.h"
#include "cminor.h"
#include "expr.h"
#include "reg.h"
#include "scope.h"
#include "str.h"
#include "symbol.h"
#include "type.h"
#include "pp_util.h"
#include "util.h"
#include "vector.h"

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

static vector_t(str_t) datastrings;

// Returns a^b
// Note: 0^x, where x < 0, is undefined, so we just return 0 (but 0^0 == 1)
static int64_t expr_pow(int64_t a, int64_t b) {
	int64_t r;

	if(b < 0)
		return 0;

	if(b == 0)
		return 1;

	r = expr_pow(a,b/2);

	return b&1 ? a*r*r : r*r;
}

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

// Evaluates a constant expression into a literal value
expr_t *expr_eval_constant(expr_t *this) {
	expr_t *head, **tail;
	expr_t *left, *right;

	if(!this)
		return NULL;

	left = expr_eval_constant(this->left);
	right = expr_eval_constant(this->right);

	for(tail = &head; this; this = this->next) {
		switch(this->op) {
		case EXPR_ADD:
			*tail = expr_create_integer(left->i + right->i);
			break;

		case EXPR_AND:
			*tail = expr_create_boolean(left->b && right->b);
			break;

		case EXPR_ASSIGN:    break;
		case EXPR_CALL:      break;
		case EXPR_DECREMENT: break;

		case EXPR_DIVIDE:
			*tail = expr_create_integer(left->i/right->i);
			break;

		case EXPR_EXPONENT:
			*tail = expr_create_integer(
				expr_pow(left->i,right->i));
			break;

		case EXPR_INCREMENT: break;

		case EXPR_MULTIPLY:
			*tail = expr_create_integer(left->i*right->i);
			break;

		case EXPR_NEGATE:
			*tail = expr_create_integer(-left->i);
			break;

		case EXPR_NOT:
			*tail = expr_create_boolean(!left->b);
			break;

		case EXPR_OR:
			*tail = expr_create_boolean(left->b || right->b);
			break;

		case EXPR_REMAINDER:
			*tail = expr_create_integer(left->i%right->b);
			break;

		case EXPR_SUBSCRIPT: break;

		case EXPR_SUBTRACT:
			*tail = expr_create_integer(left->i - right->i);
			break;

		case EXPR_EQ: *tail = expr_create_boolean(
			  left->op == EXPR_BOOLEAN ? left->b == right->b
			: left->op == EXPR_CHARACTER ? left->c == right->c
			: left->op == EXPR_INTEGER ? left->i == right->i
			: false); // Should never happen
			break;
		case EXPR_GE:
			*tail = expr_create_boolean(left->i >= right->i);
			break;
		case EXPR_GT:
			*tail = expr_create_boolean(left->i > right->i);
			break;
		case EXPR_LE:
			*tail = expr_create_boolean(left->i <= right->i);
			break;
		case EXPR_LT:
			*tail = expr_create_boolean(left->i < right->i);
			break;
		case EXPR_NE: *tail = expr_create_boolean(
			  left->op == EXPR_BOOLEAN ? left->b != right->b
			: left->op == EXPR_CHARACTER ? left->c != right->c
			: left->op == EXPR_INTEGER ? left->i != right->i
			: false); // Should never happen
			break;

		case EXPR_ARRAY:
			*tail = expr_create(EXPR_ARRAY,
				expr_eval_constant(this->left),NULL);
			break;

		case EXPR_BOOLEAN:   *tail = this; break;
		case EXPR_CHARACTER: *tail = this; break;
		case EXPR_INTEGER:   *tail = this; break;
		case EXPR_REFERENCE: break;
		case EXPR_STRING:    *tail = this; break;
		}

		if(*tail)
			tail = &(*tail)->next;
	}

	// Should never happen, because this should be caught in typechecking
	if(!head)
		die("non-constant expression passed to expr_eval_constant()");

	return head;
}

int expr_codegen(expr_t *this, FILE *f, bool wantaddr, int outindex) {
	expr_t *expr;
	size_t expri, size;
	vector_t(int) regs;
	int left, reg, right;

	if(!this)
		return -1;

	if(this->op != EXPR_ARRAY)
		left = expr_codegen(this->left,f,this->op == EXPR_ASSIGN
			|| this->op == EXPR_CALL || this->op == EXPR_DECREMENT
			|| this->op == EXPR_INCREMENT
			|| this->op == EXPR_SUBSCRIPT,0);

	if(this->op != EXPR_CALL)
		right = expr_codegen(this->right,f,false,0);

	switch(this->op) {
	case EXPR_ADD:
		fprintf(f,"add %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_AND:
		fprintf(f,"and %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_ASSIGN:
		fprintf(f,"mov %s, (%s)\n",reg_name(right),reg_name(left));
		reg_free(left);
		return right;

	case EXPR_CALL:
		vector_init(regs);

		for(expr_t *expr = this->right; expr; expr = expr->next)
			vector_append(regs,expr_codegen(expr,f,false,0));

		for(size_t i = 0; i < regs.n; i++) {
			fprintf(f,"mov %s, %s\n",
				reg_name(regs.v[i]),reg_name_arg(i));
			reg_free(regs.v[i]);
		}

		vector_free(regs);

		fputs("push %r10\n",f);
		fputs("push %r11\n",f);

		fprintf(f,"call *%s\n",reg_name(left));

		fputs("pop %r11\n",f);
		fputs("pop %r10\n",f);

		fprintf(f,"mov %%rax, %s\n",reg_name(left));
		return left;

	case EXPR_DECREMENT:
		reg = reg_alloc();
		fprintf(f,"mov $-1, %s\n",reg_name(reg));
		fprintf(f,"xadd %s, (%s)\n",reg_name(reg),reg_name(left));
		reg_free(left);
		return reg;

	case EXPR_DIVIDE:
		fprintf(f,"mov %s, %%rax\n",reg_name(left));
		fprintf(f,"cqo\n");
		fprintf(f,"idiv %s\n",reg_name(right));
		fprintf(f,"mov %%rax, %s\n",reg_name(left));
		reg_free(right);
		return left;

	case EXPR_EXPONENT:
		fprintf(f,"mov $1, %%rax\n");
		fprintf(f,"cmp $0, %s\n",reg_name(right));
		fprintf(f,"jle 0f\n");
		fprintf(f,"3: cmp $1, %s\n",reg_name(right));
		fprintf(f,"jle 2f\n");
		fprintf(f,"shr %s\n",reg_name(right));
		fprintf(f,"jnc 4f\n");
		fprintf(f,"imul %s, %%rax\n",reg_name(left));
		fprintf(f,"4: imul %s, %s\n",
			reg_name(left),reg_name(left));
		fprintf(f,"jmp 3b\n");
		fprintf(f,"0: sete %s\n",reg_name_8l(left));
		fprintf(f,"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		fprintf(f,"2: imul %%rax, %s\n",reg_name(left));
		fprintf(f,"9:\n");
		reg_free(right);
		return left;

	case EXPR_INCREMENT:
		reg = reg_alloc();
		fprintf(f,"mov $1, %s\n",reg_name(reg));
		fprintf(f,"xadd %s, (%s)\n",reg_name(reg),reg_name(left));
		reg_free(left);
		return reg;

	case EXPR_MULTIPLY:
		fprintf(f,"imul %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_NEGATE:
		fprintf(f,"neg %s\n",reg_name(left));
		return left;

	case EXPR_NOT:
		fprintf(f,"xor $1, %s\n",reg_name(left));
		return left;

	case EXPR_OR:
		fprintf(f,"or %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_REMAINDER:
		fprintf(f,"mov %s, %%rax\n",reg_name(left));
		fprintf(f,"cqo\n");
		fprintf(f,"idiv %s\n",reg_name(right));
		fprintf(f,"mov %%rdx, %s\n",reg_name(left));
		reg_free(right);
		return left;

	case EXPR_SUBSCRIPT:
		if(size = type_size(this->left->type->subtype), size > 1)
			fprintf(f,"imul $%zu, %s\n",size,reg_name(right));
		fprintf(f,"%s (%s,%s,8), %s\n",wantaddr ? "lea" : "mov",
			reg_name(left),reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_SUBTRACT:
		fprintf(f,"sub %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_EQ:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"sete %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_GE:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"setge %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_GT:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"setg %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_LE:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"setle %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_LT:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"setl %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_NE:
		fprintf(f,"cmp %s, %s\n",reg_name(right),reg_name(left));
		fprintf(f,"setne %s\n",reg_name_8l(left));
		fprintf(f,
			"movzx %s, %s\n",reg_name_8l(left),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_ARRAY:
		size = type_size(this->left->type);
		for(expr = this->left, expri = 0;
			expr; expr = expr->next, expri++) {
			reg = expr_codegen(expr,f,false,outindex - expri*size);
			if(reg >= 0) {
				fprintf(f,"mov %s, -%zu(%%rbp)\n",
					reg_name(reg),8*(outindex - expri));
				reg_free(reg);
			}
		}
		return -1;

	case EXPR_BOOLEAN:
		reg = reg_alloc();
		fprintf(f,"mov $%i, %s\n",(int) this->b,reg_name(reg));
		return reg;

	case EXPR_CHARACTER:
		reg = reg_alloc();
		fprintf(f,"mov $%i, %s\n",(int) this->c,reg_name(reg));
		return reg;

	case EXPR_INTEGER:
		reg = reg_alloc();
		fprintf(f,"mov $%"PRIi64", %s\n",this->i,reg_name(reg));
		return reg;

	case EXPR_REFERENCE:
		reg = reg_alloc();
		size = type_size(this->type);

		switch(this->symbol->level) {
		case SYMBOL_ARG:
			fprintf(f,"%s -%zu(%%rbp), %s\n",wantaddr && !type_is(
					this->symbol->type,TYPE_ARRAY)
					? "lea" : "mov",
				8*(this->symbol->index + size),reg_name(reg));
			break;

		case SYMBOL_GLOBAL:
			fprintf(f,"%s %s, %s\n",wantaddr ? "lea" : "mov",
				this->s.v,reg_name(reg));
			break;

		case SYMBOL_LOCAL:
			fprintf(f,"%s -%zu(%%rbp), %s\n",
				wantaddr ? "lea" : "mov",
				8*(this->symbol->func->type->nargs
					+ this->symbol->index + size),
				reg_name(reg));
			break;
		}
		return reg;

	case EXPR_STRING:
		reg = reg_alloc();
		fprintf(f,
			"mov $string$%zu, %s\n",datastrings.n,reg_name(reg));
		vector_append(datastrings,this->s);
		return reg;
	}

	// Should never happen
	die("reached end of expr_codegen() without returning a register");
	return -1;
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

// Special print function used when generating global variable declarations
void expr_print_asm(expr_t *this, FILE *f, bool first) {
	for(; this; this = this->next, first = false) {
		switch(this->op) {
		case EXPR_ARRAY:
			expr_print_asm(this->left,f,first);
			break;

		case EXPR_BOOLEAN:
			fprintf(f,"%s %i",first ? ".quad" : ",",(int) this->b);
			break;

		case EXPR_CHARACTER:
			fprintf(f,"%s %i",first ? ".quad" : ",",(int) this->c);
			break;

		case EXPR_INTEGER:
			fprintf(f,"%s %"PRIi64,
				first ? ".quad" : ",",this->i);
			break;

		case EXPR_STRING:
			fprintf(f,"%s string$%zu",first ? ".quad" : ",",
				datastrings.n);
			vector_append(datastrings,this->s);
			break;

		default: // Should never happen
			expr_print(this);
			die("non-literal value passed to expr_print_asm()");
		}
	}
}

void expr_print_asm_strings(FILE *f) {
	fputs(".data\n",f);

	for(size_t si = 0; si < datastrings.n; si++) {
		fprintf(f,"string$%zu: .string \"",si);

		for(size_t ci = 0; ci < datastrings.v[si].n; ci++)
			fprintf(f,"%s",datastrings.v[si].v[ci] == '\n' ? "\\n"
				: datastrings.v[si].v[ci] == '\0' ? "\\000"
				: datastrings.v[si].v[ci] == '"' ? "\\\""
				: (char []) {datastrings.v[si].v[ci], '\0'});

		fputs("\"\n",f);
	}
}

void expr_resolve(expr_t *this) {
	while(this) {
		if(this->op == EXPR_REFERENCE) {
			if(this->symbol = scope_lookup(this->s)) {
				if(cminor_mode == CMINOR_RESOLVE) {
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

void expr_typecheck(expr_t *this) {
	arg_t *arg;
	size_t m, n;
	expr_t *expr;
	bool constant, fail, moreargs;

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
				moreargs = !!arg;

				for(m = n; arg || expr;
					arg = arg ? arg->next : NULL,
					expr = expr ? expr->next : NULL, m++);

				cminor_errorcount++;
				printf("type error: call to ");
				expr_print(this->left);
				printf(" has %zu argument%s but should have "
					"%zu\n",
					moreargs ? n : m,
					(moreargs ? n : m) == 1 ? "" : "s",
					moreargs ? m : n);
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

