#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define vector_t(_T) _T##_vector_t

#define typedef_vector_t(_T) \
typedef struct { \
	size_t c, n; \
	_T *v; \
} vector_t(_T)

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

typedef_vector_t(char);
typedef_vector_t(int);

#endif

