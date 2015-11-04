#ifndef PP_UTIL_H
#define PP_UTIL_H

#include <stdlib.h>
#include <string.h>

#define CAT(a, b) a##b

#define new(_T, ...) \
	((_T *) memcpy(malloc(sizeof(_T)),&((_T) __VA_ARGS__),sizeof(_T)))

#endif

