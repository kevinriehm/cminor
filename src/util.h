#ifndef UTIL_H
#define UTIL_H

#define die(...)       die_prefixed("error: ",__VA_ARGS__)
#define scan_die(...)  die_prefixed("scan error: ",__VA_ARGS__)
#define parse_die(...) die_prefixed("parse error: ",__VA_ARGS__)

#define resolve_error(...) error_prefixed("resolve error: ",__VA_ARGS__)

extern int util_errorcount;

void die_prefixed(char *, char *, ...);
void error_prefixed(char *, char *, ...);

#endif

