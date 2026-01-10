#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#define LITH_VERSION "1.0.0"
#define LITH_SERVER_NAME "LITH-v1"

#define MAX_CONNECTIONS 100
#define READ_TIMEOUT_SEC 5

#define CHECK_ERROR(condition, msg) \
    do { \
        if (condition) { \
            perror(msg); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#endif