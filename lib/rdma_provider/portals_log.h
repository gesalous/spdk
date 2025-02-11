#ifndef PORTALS_LOG_H
#define PORTALS_LOG_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SPDK_PTL_DEBUG(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[PTL_DEBUG][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)

#define SPDK_PTL_INFO(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_INFO][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)


#define SPDK_PTL_WARN(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_WARN][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)


#define SPDK_PTL_FATAL(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_FATAL][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
        _exit(EXIT_FAILURE);   \
    } while (0)

#endif
