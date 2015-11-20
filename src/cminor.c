#include <stdio.h>
#include <string.h>

#include "cminor.h"
#include "resolve.h"
#include "scan.h"
#include "str.h"
#include "util.h"
#include "vector.h"

#include "gen/parse.tab.h"

static vector_t(str_t) inputs;

static void process_args(int argc, char **argv) {
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i],"-parse") == 0)
			cminor_mode = MODE_PARSE;
		else if(strcmp(argv[i],"-print") == 0)
			cminor_mode = MODE_PARSE;
		else if(strcmp(argv[i],"-resolve") == 0)
			cminor_mode = MODE_RESOLVE;
		else if(strcmp(argv[i],"-scan") == 0)
			cminor_mode = MODE_SCAN;
		else if(strcmp(argv[i],"-typecheck") == 0)
			cminor_mode = MODE_TYPECHECK;
		else vector_append(inputs,str_new(argv[i],strlen(argv[i])));
	}
}

static FILE *open_input_file(char *name) {
	FILE *f;

	if(f = fopen(name,"r"), !f)
		die("cannot open '%s' for reading",name);

	return f;
}

int main(int argc, char **argv) {
	cminor_mode = MODE_NONE;

	vector_init(inputs);

	process_args(argc,argv);

	switch(cminor_mode) {
	case MODE_PARSE:
		if(inputs.n != 1)
			die("parser mode requires exactly one input file");

		parse(open_input_file(inputs.v[0].v));
		break;

	case MODE_RESOLVE:
		if(inputs.n != 1)
			die("resolution mode requires exactly one input file");

		parse(open_input_file(inputs.v[0].v));
		resolve();
		break;

	case MODE_SCAN:
		if(inputs.n != 1)
			die("scanner mode requires exactly one input file");

		scan(open_input_file(inputs.v[0].v));
		break;

	default: die("unknown or unhandled mode (%i)",cminor_mode);
	}

	return !!util_errorcount;
}

