#ifndef _JULIA_UTIL_H_
#define _JULIA_UTIL_H_

#define OK                  (0)
#define AGAIN               (1)
#define EMPTY_LINE          (2)
#define ERR_INVALID_REQUEST (-2)
#define ERR_INVALID_METHOD  (-3)
#define ERR_INVALID_VERSION (-4)
#define ERR_INTERNAL_ERROR  (-5)
#define ERR_INVALID_HEADER  (-6)
#define ERR_INVALID_URI     (-7)

#define ERR_STATUS(status)  (-status)

#define CRLF                "\r\n"

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
