#ifndef CMINOR_H
#define CMINOR_H

enum {
	MODE_NONE,
	MODE_PARSE,
	MODE_RESOLVE,
	MODE_SCAN,
	MODE_TYPECHECK
} cminor_mode;

extern int cminor_errorcount;

#endif

