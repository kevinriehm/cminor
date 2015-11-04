#ifndef SCAN_H
#define SCAN_H

#include <stdio.h>

FILE *yyin;

int currentline;

void scan();
int yylex();

#endif

