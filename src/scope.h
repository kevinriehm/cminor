#ifndef SCOPE_H
#define SCOPE_H

#include <stdbool.h>

#include "str.h"

struct symbol;

void scope_enter(bool);
void scope_leave();

bool scope_is_global();
size_t scope_num_function_locals();

void scope_bind(str_t, struct symbol *);
struct symbol *scope_lookup(str_t);
struct symbol *scope_lookup_local(str_t);

#endif

