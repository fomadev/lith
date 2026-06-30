/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "server_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lith_close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

bool is_safe_path(const char *path) {
    if (!path) return false;

    // Blocage basique mais robuste contre le Directory Traversal
    if (strstr(path, "..") != NULL) {
        return false;
    }
    return true;
}

char* read_file(const char* filename, long *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(*size + 1);
    if (content) {
        size_t read_elements = fread(content, 1, *size, f);
        content[read_elements] = '\0';
    }
    fclose(f);
    return content;
}