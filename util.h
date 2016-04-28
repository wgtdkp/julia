#ifndef _JULIA_UTIL_H_
#define _JULIA_UTIL_H_

//#define MIN(x, y)   ((x) < (y) ? (x): (y))
//#define MAX(x, y)   ((x) > (y) ? (x): (y))
//#define true    (1)
//#define false   (0)

//typedef unsigned char bool;

static inline int min(int x, int y)
{
    return x < y ? x: y;
}

static inline int max(int x, int y)
{
    return x > y ? x: y;
}

void ju_log(const char* format, ...);

#endif
