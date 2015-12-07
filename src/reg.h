#ifndef REG_H
#define REG_H

#include <stdio.h>
#include <stdlib.h>

#include "str.h"
#include "vector.h"

typedef enum {
	REG_RAX,
	REG_RBX,
	REG_RCX,
	REG_RDX,
	REG_RSI,
	REG_RDI,
	REG_RBP,
	REG_RSP,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_R13,
	REG_R14,
	REG_R15,

	REG_NONE
} reg_real_t;

typedef_vector_t(reg_real_t);

void reg_reset(void);
size_t reg_frame_size(void);

int reg_alloc(FILE *);
int reg_assign_array(size_t);
int reg_assign_function(str_t);
int reg_assign_global(str_t);
int reg_assign_local(int);
int reg_assign_real(reg_real_t);
int reg_assign_subscript(int, size_t);

void reg_free(int);
void reg_free_persistent(int);

void reg_block_enter(void);
void reg_block_leave(void);

void reg_record_lvalues(void);
void reg_restore_lvalues(FILE *f);

bool reg_is_real(int);
bool reg_is_persistent(int);

int *reg_get_lvalue(int);
void reg_set_lvalue(int, int *);

void reg_make_real(int, FILE *);
void reg_make_one_real(int, int, FILE *);
void reg_make_temporary(int *, FILE *);
void reg_make_one_temporary(int *, int *, FILE *, bool *);
void reg_make_persistent(int);

void reg_hint(reg_real_t);
void reg_map_v(int, int *, reg_real_t *, FILE *);
void reg_vacate_v(int, reg_real_t *, FILE *);

char *reg_name(int);
char *reg_name_8l(int);
char *reg_name_real(reg_real_t);

#endif

