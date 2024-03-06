#ifndef __M_LOGGER_H__
#define __M_LOGGER_H__
#include <stdio.h>
#include <time.h>
enum
{
    INF,
    DBG,
    ERR
};
#define DEFAULT_LOG_LEVEL DBG
#define LOG(level, format, ...)                                                               \
    do                                                                                        \
    {                                                                                         \
        if (DEFAULT_LOG_LEVEL > level)                                                        \
            break;                                                                            \
        time_t t = time(NULL);                                                                \
        struct tm *lt = localtime(&t);                                                        \
        char buffer[32] = {0};                                                                \
        strftime(buffer, 31, "%H:%M:%S", lt);                                                 \
        fprintf(stdout, "[%s %s:%d]" format "\n", buffer, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define ILOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#endif