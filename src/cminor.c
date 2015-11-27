#include <stdio.h>
#include <string.h>

#include "cminor.h"
#include "codegen.h"
#include "resolve.h"
#include "scan.h"
#include "str.h"
#include "typecheck.h"
#include "util.h"
#include "vector.h"

#include "gen/parse.tab.h"

static vector_t(str_t) files;

int cminor_errorcount = 0;

static void process_args(int argc, char **argv) {
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i],"-codegen") == 0)
			cminor_mode = CMINOR_CODEGEN;
		else if(strcmp(argv[i],"-parse") == 0)
			cminor_mode = CMINOR_PARSE;
		else if(strcmp(argv[i],"-print") == 0)
			cminor_mode = CMINOR_PARSE;
		else if(strcmp(argv[i],"-resolve") == 0)
			cminor_mode = CMINOR_RESOLVE;
		else if(strcmp(argv[i],"-scan") == 0)
			cminor_mode = CMINOR_SCAN;
		else if(strcmp(argv[i],"-typecheck") == 0)
			cminor_mode = CMINOR_TYPECHECK;
		else vector_append(files,str_new(argv[i],strlen(argv[i])));
	}
}

static FILE *open_input_file(char *name) {
	FILE *f;

	if(f = fopen(name,"r"), !f)
		die("cannot open '%s' for reading",name);

	return f;
}

static FILE *open_output_file(char *name) {
	FILE *f;

	if(f = fopen(name,"w"), !f)
		die("cannot open '%s' for reading",name);

	return f;
}

int main(int argc, char **argv) {
	cminor_mode = CMINOR_NONE;

	vector_init(files);

	process_args(argc,argv);

	switch(cminor_mode) {
	case CMINOR_CODEGEN:
		if(files.n != 2)
			die("codegen mode requires exactly one input and one "
				"output file");

		parse(open_input_file(files.v[0].v));
		resolve();

		if(cminor_errorcount)
			break;

		typecheck();

		if(cminor_errorcount)
			break;

		codegen(open_output_file(files.v[1].v));
		break;

	case CMINOR_PARSE:
		if(files.n != 1)
			die("parser mode requires exactly one input file");

		parse(open_input_file(files.v[0].v));
		break;

	case CMINOR_RESOLVE:
		if(files.n != 1)
			die("resolver mode requires exactly one input file");

		parse(open_input_file(files.v[0].v));
		resolve();
		break;

	case CMINOR_SCAN:
		if(files.n != 1)
			die("scanner mode requires exactly one input file");

		scan(open_input_file(files.v[0].v));
		break;

	case CMINOR_TYPECHECK:
		if(files.n != 1)
			die("typechecker mode requires exactly one input "
				"file");

		parse(open_input_file(files.v[0].v));
		resolve();

		if(cminor_errorcount)
			break;

		typecheck();
		break;

	default: die("unknown or unhandled mode (%i)",cminor_mode);
	}

	if(cminor_errorcount)
		fprintf(stderr,"fatal: %i error%s\n",cminor_errorcount,
			cminor_errorcount == 1 ? "" : "s");

	return !!cminor_errorcount;
}

