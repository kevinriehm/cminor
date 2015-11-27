#ifndef CMINOR_H
#define CMINOR_H

enum {
	CMINOR_NONE,
	CMINOR_CODEGEN,
	CMINOR_PARSE,
	CMINOR_RESOLVE,
	CMINOR_SCAN,
	CMINOR_TYPECHECK
} cminor_mode;

extern int cminor_errorcount;

#endif

