#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

int util_errorcount = 0;

void die_prefixed(char *prefix, char *msg, ...) {
	va_list ap;

	fputs(prefix,stderr);

	va_start(ap,msg);
	vfprintf(stderr,msg,ap);
	va_end(ap);

	fputc('\n',stderr);

	exit(EXIT_FAILURE);
}

void error_prefixed(char *prefix, char *msg, ...) {
	va_list ap;

	util_errorcount++;

	fputs(prefix,stderr);

	va_start(ap,msg);
	vfprintf(stderr,msg,ap);
	va_end(ap);

	fputc('\n',stderr);
}

