#ifndef _JULIA_UTIL_H_
#define _JULIA_UTIL_H_

#define OK  (0)
#define ERR_INVALID_REQUEST (-1)
#define ERR_INVALID_METHOD  (-2)
#define ERR_INVALID_VERSION (-3)
#define ERR_INTERNAL_ERROR  (-4)

#define EXIT_ON(cond, msg)  \
do {                        \
    if (cond)               \
        perror(msg);        \
} while (0)


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
