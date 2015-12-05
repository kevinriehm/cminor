#ifndef STR_H
#define STR_H

#include <stdbool.h>
#include <stdlib.h>

#include "vector.h"

typedef vector_t(char) str_t;

typedef_vector_t(str_t);

str_t str_new(char *, size_t);

void str_append_c(str_t *, char);
void str_ensure_cap(str_t *, size_t);
bool str_eq(str_t, str_t);
void str_resize(str_t *, size_t);

#endif

