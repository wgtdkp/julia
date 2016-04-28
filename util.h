#ifndef _JULIA_UTIL_H_
#define _JULIA_UTIL_H_


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
