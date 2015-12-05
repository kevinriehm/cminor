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

void str_ensure_cap(str_t *this, size_t mincap) {
	if(this->c < mincap)
		str_resize(this,mincap);
}

bool str_eq(str_t a, str_t b) {
	return a.n == b.n && memcmp(a.v,b.v,a.n) == 0;
}

void str_resize(str_t *this, size_t size) {
	vector_resize(*this,size + 1);
}

