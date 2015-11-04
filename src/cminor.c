#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "str.h"
#include "util.h"
#include "vector.h"

#include "gen/parse.tab.h"

static enum {
	MODE_COMPILE,
	MODE_PARSE,
	MODE_SCAN
} mode;

static vector_t(str_t) inputs;

static void process_args(int argc, char **argv) {
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i],"-parse") == 0)
			mode = MODE_PARSE;
		else if(strcmp(argv[i],"-scan") == 0)
			mode = MODE_SCAN;
		else vector_append(inputs,str_new(argv[i],strlen(argv[i])));
	}
}

int main(int argc, char **argv) {
	FILE *f;

	mode = MODE_COMPILE;

	vector_init(inputs);

	process_args(argc,argv);

	switch(mode) {
	case MODE_PARSE:
		if(inputs.n != 1)
			die("parser mode requires exactly one input file");

		if(f = fopen(inputs.v[0].v,"r"), !f)
			die("cannot open '%s' for reading",inputs.v[0].v);

		parse(f);
		break;

	case MODE_SCAN:
		if(inputs.n != 1)
			die("scanner mode requires exactly one input file");

		if(f = fopen(inputs.v[0].v,"r"), !f)
			die("cannot open '%s' for reading",inputs.v[0].v);

		scan(f);
		break;

	default: die("unknown or unhandled mode (%i)",mode);
	}

	return 0;
}

