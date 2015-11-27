#ifndef EXPR_H
#define EXPR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "str.h"

typedef enum {
	EXPR_ADD,
	EXPR_AND,
	EXPR_ASSIGN,
	EXPR_CALL,
	EXPR_DECREMENT,
	EXPR_DIVIDE,
	EXPR_EXPONENT,
	EXPR_INCREMENT,
	EXPR_MULTIPLY,
	EXPR_NEGATE,
	EXPR_NOT,
	EXPR_OR,
	EXPR_REMAINDER,
	EXPR_SUBSCRIPT,
	EXPR_SUBTRACT,

	EXPR_EQ,
	EXPR_GE,
	EXPR_GT,
	EXPR_LE,
	EXPR_LT,
	EXPR_NE,

	EXPR_ARRAY,
	EXPR_BOOLEAN,
	EXPR_CHARACTER,
	EXPR_INTEGER,
	EXPR_REFERENCE,
	EXPR_STRING
} expr_op_t;

typedef struct expr {
	expr_op_t op;

	bool b;
	char c;
	int64_t i;
	str_t s;

	struct symbol *symbol;
	struct type *type;

	struct expr *left;
	struct expr *right;

	struct expr *parent;
	struct expr *next;
} expr_t;

expr_t *expr_create(expr_op_t, expr_t *, expr_t *);
expr_t *expr_create_boolean(bool);
expr_t *expr_create_character(char);
expr_t *expr_create_integer(int64_t);
expr_t *expr_create_reference(str_t);
expr_t *expr_create_string(str_t);

expr_t *expr_eval_constant(expr_t *);

int expr_codegen(expr_t *, FILE *, bool);
void expr_print(expr_t *);
void expr_print_asm(expr_t *, FILE *, bool);
void expr_print_asm_strings(FILE *);
void expr_resolve(expr_t *);
void expr_type_print(expr_t *);
void expr_typecheck(expr_t *);

#endif

