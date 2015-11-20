#ifndef HTABLE_H
#define HTABLE_H

#include <stdbool.h>

#include "str.h"

#define HTABLE_LOAD_FACTOR   0.7
#define HTABLE_GROWTH_FACTOR 1.5

#define htable_t(_T)     _T##_htable_t
#define htable_bin(_T)   _T##_htable_bin
#define htable_bin_t(_T) _T##_htable_bin_t

#define typedef_htable_t(_T) \
typedef struct htable_bin(_T) { \
	htable_bin_header_t head; \
	_T *val; \
} htable_bin_t(_T); \
\
typedef struct { \
	size_t n; \
	size_t nbins; \
	htable_bin_t(_T) **v; \
} htable_t(_T)

#define htable_new(_T) (htable_t(_T)) { \
	.nbins = 8, \
	.n = 0, \
	.v = calloc(8,sizeof(htable_bin_t(_T) **)) \
}

// Inserts _val into _this under _key, and returns whether _key already had an
// associated value
#define htable_insert(_this, _key, _val) \
	(htable_locate((_this).nbins,(htable_bin_header_t **) (_this).v, \
			(_key),true,sizeof **(_this).v) \
		? ((_this).v[htable_hash((_this).nbins,(_key))]->val \
				= (_val), \
			true) \
		: ((_this).v[htable_hash((_this).nbins,(_key))]->val \
				= (_val), \
			((_this).n++ > HTABLE_LOAD_FACTOR*(_this).nbins \
				? htable_resize(&(_this).nbins, \
					(htable_bin_header_t ***) &(_this).v, \
					HTABLE_GROWTH_FACTOR*(_this).nbins) \
				: (void) 0), \
			false))

// Returns a pointer to the value stored under _key, or NULL if _key is not in
// _this
#define htable_lookup(_this, _key) \
	(htable_locate((_this).nbins,(htable_bin_header_t **) (_this).v, \
			(_key),false,0) \
		? (_this).v[htable_hash((_this).nbins,(_key))]->val \
		: NULL)

typedef struct htable_bin_header {
	struct htable_bin_header *next;
	str_t key;
} htable_bin_header_t;

// Private
size_t htable_hash(size_t, str_t);
void htable_resize(size_t *, htable_bin_header_t ***, size_t);
bool htable_locate(size_t, htable_bin_header_t **, str_t, bool, size_t);

#endif

