#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>

void ju_error(const char* format, ...) {
    fprintf(stderr, "error: ");
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

void ju_log(const char* format, ...) {
    time_t t = time(NULL);
    FILE* log_file = fopen(INSTALL_DIR "julia.log", "a+");
    if (log_file == NULL) {
        return;
    }
    struct tm tm = *localtime(&t);
    fprintf(log_file, "[%4d: %02d: %02d: %02d:%02d:%02d] [pid: %5d]",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, getpid());

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fprintf(log_file, "\n");
    fclose(log_file);
}

