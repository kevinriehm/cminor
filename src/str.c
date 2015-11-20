#include <string.h>

#include "str.h"

str_t str_new(char *p, size_t len) {
	return (str_t) {
		.c = len,
		.n = len,
		.v = memcpy(calloc(1,len + 1),p,len)
	};
}

void str_append_c(str_t *this, char c) {
	vector_append(*this,'\0');
	this->v[this->n - 1] = c;
}

bool str_eq(str_t a, str_t b) {
	return a.n == b.n && memcmp(a.v,b.v,a.n) == 0;
}

