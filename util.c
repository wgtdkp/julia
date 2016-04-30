#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


void ju_log(const char* format, ...)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char file_name[64];
    snprintf(file_name, 64 - 1, "log-%d-%d-%d.txt",
            tm.tm_year, tm.tm_mon + 1, tm.tm_mday);

    FILE* log_file = fopen(file_name, "a+");
    if (log_file == NULL)
        return;

    fprintf(log_file, "[%d:%d:%d] ", tm.tm_hour, tm.tm_min, tm.tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fclose(log_file);
}

