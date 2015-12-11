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

	if(!this || !this->type->constant)
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

int expr_codegen(expr_t *this, FILE *f, bool wantlvalue, int outreg) {
	static int nlabels = 0;

	int label;
	expr_t *expr;
	vector_t(int) regs;
	reg_real_t *realregs;
	size_t expri, nargs, size;
	int left, *lvalue, reg, right, subreg;

	if(!this)
		return -1;

	if(this->op != EXPR_ARRAY)
		left = expr_codegen(this->left,f,this->op == EXPR_ASSIGN
			|| this->op == EXPR_DECREMENT
			|| this->op == EXPR_INCREMENT
			|| this->op == EXPR_SUBSCRIPT,0);

	if(this->op != EXPR_AND
		&& this->op != EXPR_CALL && this->op != EXPR_OR)
		right = expr_codegen(this->right,f,false,-1);

	switch(this->op) {
	case EXPR_ADD:
		reg_make_one_temporary(&left,&right,f,NULL);
		fprintf(f,"\tadd %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_AND:
		label = nlabels++;

		reg_record_lvalues();

		fprintf(f,"\tcmp $0, %s\n",reg_name_8l(left));
		fprintf(f,"\tje .Lexpr_%i\n",label);

		right = expr_codegen(this->right,f,false,-1);
		reg_make_one_temporary(&left,&right,f,NULL);
		fprintf(f,"\tand %s, %s\n",reg_name(right),reg_name(left));

		reg_restore_lvalues(f);
		fprintf(f,"\t.Lexpr_%i:\n",label);

		reg_free(right);
		return left;

	case EXPR_ASSIGN:
		if(lvalue = reg_get_lvalue(left)) {
			reg_free_persistent(left);
			reg_make_temporary(&right,f);
			*lvalue = right;
			reg_make_persistent(right);
			reg_set_lvalue(right,lvalue);
		} else {
			reg_make_one_real(left,right,f);
			fprintf(f,"\tmov %s, %s%s\n",
				reg_name(right),reg_name(left),
				reg_is_pointer(left) ? ")" : "");
			reg_free(left);
		}
		return right;

	case EXPR_CALL:
		vector_init(regs);

		// Push arguments past the sixth onto the stack
		nargs = arg_count(this->left->type->args);
		if(nargs > 6) {
			if(nargs&1)
				fputs("\tsub $8, %rsp\n",f);

			expr_codegen_push_args(this->right->next->next->next
				->next->next->next,f);
		}

		// Generate the first six or fewer arguments
		realregs = (reg_real_t []) {
			REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9,
			REG_RAX, REG_R10, REG_R11
		};

		for(expr_t *expr = this->right;
			expr && regs.n < 6; expr = expr->next) {
			reg_hint(realregs[regs.n]);
			reg = expr_codegen(expr,f,false,-1);
			reg_make_temporary(&reg,f);

			vector_append(regs,reg);
		}

		while(regs.n < 9)
			vector_append(regs,-1);

		reg_map_v(9,regs.v,realregs,f);

		fprintf(f,"\tcall %s\n",reg_name(left));
		reg_free(left);

		for(size_t i = 0; i < regs.n; i++)
			reg_free(regs.v[i]);

		vector_free(regs);

		return reg_assign_real(REG_RAX);

	case EXPR_DECREMENT:
		reg = reg_alloc(f);
		fprintf(f,"\tmov $-1, %s\n",reg_name(reg));
		fprintf(f,"\txadd %s, %s%s\n",reg_name(reg),
			reg_name(left),reg_is_pointer(left) ? ")" : "");
		reg_free(left);
		return reg;

	case EXPR_DIVIDE:
		reg_make_temporary(&left,f);
		reg_vacate_v(2,(reg_real_t []) {REG_RAX, REG_RDX},f);
		fprintf(f,"\tmov %s, %%rax\n",reg_name(left));
		fprintf(f,"\tcqo\n");
		fprintf(f,"\tidivq %s\n",reg_name(right));
		fprintf(f,"\tmov %%rax, %s\n",reg_name(left));
		reg_free(right);
		return left;

	case EXPR_EXPONENT:
		reg = reg_alloc(f);
		reg_make_temporary(&left,f);
		reg_make_temporary(&right,f);

		fprintf(f,"\tmov $1, %s\n",reg_name(reg));
		fprintf(f,"\tcmp $0, %s\n",reg_name(right));
		fprintf(f,"\tjle 0f\n");
		fprintf(f,"3:\tcmp $1, %s\n",reg_name(right));
		fprintf(f,"\tjle 2f\n");
		fprintf(f,"\tshr %s\n",reg_name(right));
		fprintf(f,"\tjnc 4f\n");
		fprintf(f,"\timul %s, %s\n",reg_name(left),reg_name(reg));
		fprintf(f,"4:\timul %s, %s\n",reg_name(left),reg_name(left));
		fprintf(f,"\tjmp 3b\n");
		fprintf(f,"0:\tsete %s\n",reg_name_8l(left));
		fprintf(f,"\tmovzx %s, %s\n",reg_name_8l(left),reg_name(left));
		fprintf(f,"2:\timul %s, %s\n",reg_name(reg),reg_name(left));
		fprintf(f,"9:\n");

		reg_free(reg);
		reg_free(right);
		return left;

	case EXPR_INCREMENT:
		reg = reg_alloc(f);
		fprintf(f,"\tmov $1, %s\n",reg_name(reg));
		fprintf(f,"\txadd %s, %s%s\n",reg_name(reg),
			reg_name(left),reg_is_pointer(left) ? ")" : "");
		reg_free(left);
		return reg;

	case EXPR_MULTIPLY:
		reg_make_one_temporary(&left,&right,f,NULL);
		fprintf(f,"\timul %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_NEGATE:
		reg_make_temporary(&left,f);
		fprintf(f,"\tneg %s\n",reg_name(left));
		return left;

	case EXPR_NOT:
		reg_make_temporary(&left,f);
		fprintf(f,"\txor $1, %s\n",reg_name(left));
		return left;

	case EXPR_OR:
		label = nlabels++;

		reg_record_lvalues();

		fprintf(f,"\tcmp $0, %s\n",reg_name_8l(left));
		fprintf(f,"\tjne .Lexpr_%i\n",label);

		right = expr_codegen(this->right,f,false,-1);
		reg_make_one_temporary(&left,&right,f,NULL);
		fprintf(f,"\tor %s, %s\n",reg_name(right),reg_name(left));

		reg_restore_lvalues(f);
		fprintf(f,"\t.Lexpr_%i:\n",label);

		reg_free(right);
		return left;

	case EXPR_REMAINDER:
		reg_make_temporary(&left,f);
		reg_vacate_v(2,(reg_real_t []) {REG_RAX, REG_RDX},f);
		fprintf(f,"\tmov %s, %%rax\n",reg_name(left));
		fprintf(f,"\tcqo\n");
		fprintf(f,"\tidivq %s\n",reg_name(right));
		fprintf(f,"\tmov %%rdx, %s\n",reg_name(left));
		reg_free(right);
		return left;

	case EXPR_SUBSCRIPT:
		reg_make_real(left,f);
		reg_make_temporary(&right,f);
		reg_make_real(right,f);
		if(size = type_size(this->left->type->subtype), size > 1)
			fprintf(f,"\timul $%zu, %s\n",size,reg_name(right));
		fprintf(f,"\t%s %s,%s,8), %s\n",wantlvalue ? "lea" : "mov",
			reg_name(left),reg_name(right),reg_name(right));
		reg_free(left);
		return wantlvalue ? reg_assign_pointer(right) : right;

	case EXPR_SUBTRACT:
		reg_make_temporary(&left,f);
		fprintf(f,"\tsub %s, %s\n",reg_name(right),reg_name(left));
		reg_free(right);
		return left;

	case EXPR_EQ:
	case EXPR_GE:
	case EXPR_GT:
	case EXPR_LE:
	case EXPR_LT:
	case EXPR_NE:
		return expr_codegen_compare(this,f,left,right);

	case EXPR_ARRAY:
		size = type_size(this->left->type);
		for(expr = this->left, expri = 0;
			expr; expr = expr->next, expri++) {
			subreg = reg_assign_subscript(outreg,expri*size);
			reg = expr_codegen(expr,f,false,subreg);

			if(reg >= 0) {
				reg_make_real(reg,f);
				fprintf(f,"\tmov %s, %s\n",
					reg_name(reg),reg_name(subreg));
				reg_free(reg);
			}

			reg_free(subreg);
		}
		return -1;

	case EXPR_BOOLEAN:
		reg = reg_alloc(f);
		fprintf(f,"\tmov $%i, %s\n",(int) this->b,reg_name(reg));
		return reg;

	case EXPR_CHARACTER:
		reg = reg_alloc(f);
		fprintf(f,"\tmov $%i, %s\n",(int) this->c,reg_name(reg));
		return reg;

	case EXPR_INTEGER:
		reg = reg_alloc(f);
		fprintf(f,"\tmov $%"PRIi64", %s\n",this->i,reg_name(reg));
		return reg;

	case EXPR_REFERENCE:
		switch(this->symbol->level) {
		case SYMBOL_ARG:
			if(type_is(this->type,TYPE_ARRAY))
				return reg_assign_pointer(this->symbol->reg);

			return this->symbol->reg;

		case SYMBOL_GLOBAL:
			if(type_is(this->type,TYPE_ARRAY)) {
				reg = reg_alloc(f);
				fprintf(f,"\tlea %s(%%rip), %s\n",
					this->s.v,reg_name(reg));
				return reg_assign_pointer(reg);
			}

			if(type_is(this->type,TYPE_FUNCTION))
				return reg_assign_function(this->s);

			return reg_assign_global(this->s);

		case SYMBOL_LOCAL:
			if(type_is(this->type,TYPE_ARRAY) && !wantlvalue) {
				reg = reg_alloc(f);
				fprintf(f,"\tlea %s), %s\n",
					reg_name(this->symbol->reg),
					reg_name(reg));
				return reg;
			}

			return this->symbol->reg;
		}

		break;

	case EXPR_STRING:
		reg = reg_alloc(f);
		fprintf(f,"\tlea string$%zu(%%rip), %s\n",
			datastrings.n,reg_name(reg));
		vector_append(datastrings,this->s);
		return reg;
	}

	// Should never happen
	die("reached end of expr_codegen() without returning a register");
	return -1;
}

int expr_codegen_compare(expr_t *this, FILE *f, int left, int right) {
	static char *suffixes[] = {
		[EXPR_EQ] = "e",
		[EXPR_GE] = "ge",
		[EXPR_GT] = "g",
		[EXPR_LE] = "le",
		[EXPR_LT] = "l",
		[EXPR_NE] = "ne"
	};

	bool swapped;

	reg_make_one_temporary(&left,&right,f,&swapped);

	if(type_is(this->left->type,TYPE_STRING)) {
		reg_vacate_v(4,(reg_real_t []) {
			REG_RAX, REG_RCX, REG_RSI, REG_RDI},f);

		// String length
		fprintf(f,"\txor %%al, %%al\n");
		fprintf(f,"\tmov $-1, %%rcx\n");
		fprintf(f,"\tmov %s, %%rdi\n",reg_name(left));
		fprintf(f,"\trepne scasb\n");

		// Compare strings
		fprintf(f,"\tmov %%rdi, %%rcx\n");
		fprintf(f,"\tsub %s, %%rcx\n",reg_name(left));
		fprintf(f,"\tsub %%rcx, %%rdi\n");
		fprintf(f,"\tmov %s, %%rsi\n",reg_name(right));
		fprintf(f,"\trepe cmpsb\n");
	} else fprintf(f,"\tcmp %s, %s\n",reg_name(swapped ? left : right),
		reg_name(swapped ? right : left));

	fprintf(f,"\tset%s %s\n",suffixes[this->op],reg_name_8l(left));
	fprintf(f,"\tmovzx %s, %s\n",reg_name_8l(left),reg_name(left));

	reg_free(right);

	return left;
}

// Push arguments onto the stack from right to left
void expr_codegen_push_args(expr_t *arg, FILE *f) {
	int reg;

	if(!arg)
		return;

	expr_codegen_push_args(arg->next,f);

	reg = expr_codegen(arg,f,false,-1);
	fprintf(f,"\tpush %s\n",reg_name(reg));
	reg_free(reg);
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
	fputs("\t.data\n",f);

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
			this->type
				= type_create(TYPE_INTEGER,0,NULL,NULL,false);
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
			this->type->lvalue = true;
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

