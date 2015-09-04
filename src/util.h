#ifndef UTIL_H
#define UTIL_H

#define die(...)      die_prefixed("error: ",__VA_ARGS__)
#define scan_die(...) die_prefixed("scan error: ",__VA_ARGS__)

void die_prefixed(char *, char *, ...);

#endif

