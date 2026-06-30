/* Copyright (c) 2026 Fordi / FomaDev */
#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "common.h"
#include "server.h"

void lith_close_socket(socket_t s);
bool is_safe_path(const char *path);
char* read_file(const char* filename, long *size);

#endif