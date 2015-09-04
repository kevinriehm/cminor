#ifndef STR_H
#define STR_H

#include <stdlib.h>

#include "vector.h"

#define str_new(_p, _len) (str_t) { \
	.c = (_len), \
	.n = (_len), \
	.v = memcpy(calloc(1,(_len) + 1),(_p),(_len)) \
}

#define str_append_c(_this, _c) do { \
	vector_append((_this),'\0'); \
	(_this).v[(_this).n - 1] = (_c); \
} while(0)

typedef vector_t(char) str_t;

typedef_vector_t(str_t);

#endif

