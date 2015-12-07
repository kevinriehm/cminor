#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#include "pp_util.h"

#define vector_t(_T) CAT(_T, _vector_t)

#define typedef_vector_t(_T) \
typedef struct { \
	size_t c, n; \
	_T *v; \
} vector_t(_T)

#define vector(_T, ...) (vector_t(_T)) { \
	.c = sizeof (__VA_ARGS__)/sizeof *(__VA_ARGS__), \
	.n = sizeof (__VA_ARGS__)/sizeof *(__VA_ARGS__), \
	.v = memcpy(malloc(sizeof (__VA_ARGS__)),(__VA_ARGS__)) \
}

#define vector_init(_this) do { \
	(_this).c = (_this).n = 0; \
	(_this).v = NULL; \
} while(0)

#define vector_free(_this) do { \
	free((_this).v); \
} while(0)

#define vector_append(_this, ...) do { \
	if((_this).n + 1 > (_this).c) { \
		(_this).c = 1.5*((_this).c + 1); \
		(_this).v = realloc((_this).v,(_this).c*sizeof *(_this).v); \
	} \
\
	(_this).v[(_this).n++] = (__VA_ARGS__); \
} while(0)

#define vector_resize(_this, _size) do { \
	(_this).c = (_this).n = (_size); \
	(_this).v = realloc((_this).v,(_this).c*sizeof *(_this).v); \
} while(0)

typedef_vector_t(char);
typedef_vector_t(int);

#endif

