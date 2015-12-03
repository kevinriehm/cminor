#ifndef SCOPE_H
#define SCOPE_H

#include <stdbool.h>

#include "str.h"

struct decl;
struct symbol;

void scope_enter(struct decl *);
void scope_leave(void);

bool scope_is_global(void);
struct decl *scope_function(void);
size_t scope_num_function_locals(void);

void scope_bind(str_t, struct symbol *);
struct symbol *scope_lookup(str_t);
struct symbol *scope_lookup_local(str_t);

#endif

