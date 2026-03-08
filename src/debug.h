#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#ifdef DEBUG
#define DPRINT(...) printf(__VA_ARGS__)
#else
#define DPRINT(...) 
#endif

#endif 
