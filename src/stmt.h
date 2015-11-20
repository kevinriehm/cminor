#ifndef STMT_H
#define STMT_H

typedef enum {
	STMT_BLOCK,
	STMT_DECL,
	STMT_EXPR,
	STMT_FOR,
	STMT_IF_ELSE,
	STMT_PRINT,
	STMT_RETURN
} stmt_op_t;

typedef struct stmt {
	stmt_op_t op;
	struct decl *decl;
	struct expr *init_expr;
	struct expr *expr;
	struct expr *next_expr;
	struct stmt *body;
	struct stmt *else_body;

	struct stmt *next;
} stmt_t;

stmt_t *stmt_create(stmt_op_t, struct decl *, struct expr *, struct expr *,
	struct expr *, stmt_t *, stmt_t *);

void stmt_print(stmt_t *, int);
void stmt_resolve(stmt_t *);
void stmt_typecheck(stmt_t *, struct decl *);

#endif

