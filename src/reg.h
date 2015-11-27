#ifndef REG_H
#define REG_H

int reg_alloc(void);
void reg_free(int);

char *reg_name(int);
char *reg_name_8l(int);
char *reg_name_arg(int);

#endif

