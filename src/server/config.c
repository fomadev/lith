/* * Copyright (c) 2026 Fordi / FomaDev. 
 * Licensed under FomaDev Public License.
 * See LICENSE file in the project root for full license information.
 */

#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int load_config(const char *filename, ServerConfig *config) {
    // Valeurs de secours par défaut
    config->port = 8090;
    strcpy(config->public_dir, "public");

    FILE *f = fopen(filename, "r");
    if (!f) {
        lith_log(LOG_WARN, "Configuration file '%s' not found. Using defaults (Port: %d, Root: %s)", 
                 filename, config->port, config->public_dir);
        return -1;
    }

    char line[384];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        char *ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0' || *ptr == '#') continue;

        char *equal = strchr(ptr, '=');
        if (!equal) continue;

        *equal = '\0';
        char *key = ptr;
        char *value = equal + 1;

        // Nettoyage de la clé
        while (isspace((unsigned char)*key)) key++;
        char *end_key = key + strlen(key) - 1;
        while (end_key > key && isspace((unsigned char)*end_key)) { *end_key = '\0'; end_key--; }

        // Nettoyage de la valeur
        while (isspace((unsigned char)*value)) value++;
        char *end_val = value + strlen(value) - 1;
        while (end_val > value && isspace((unsigned char)*end_val)) { *end_val = '\0'; end_val--; }

        if (strcmp(key, "PORT") == 0) {
            config->port = atoi(value);
        } else if (strcmp(key, "PUBLIC_DIR") == 0) {
            strncpy(config->public_dir, value, sizeof(config->public_dir) - 1);
            config->public_dir[sizeof(config->public_dir) - 1] = '\0';
        }
    }

    fclose(f);
    lith_log(LOG_INFO, "Configuration loaded: Port=%d, Public Directory='%s'", config->port, config->public_dir);
    return 0;
}