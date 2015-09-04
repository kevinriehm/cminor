#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define vector_t(T) T##_vector_t

#define typedef_vector_t(T) \
typedef struct { \
	size_t c, n; \
	T *v; \
} vector_t(T)

#define vector_init(_this) do { \
	(_this).c = (_this).n = 0; \
	(_this).v = NULL; \
} while(0)

#define vector_append(_this, ...) do { \
	if((_this).n + 1 > (_this).c) { \
		(_this).c = 1.5*((_this).c + 1); \
		(_this).v = realloc((_this).v,(_this).c*sizeof *(_this).v); \
	} \
\
	(_this).v[(_this).n++] = (__VA_ARGS__); \
} while(0)

typedef_vector_t(char);

#endif

