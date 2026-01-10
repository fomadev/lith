#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

// Couleurs ANSI pour un terminal lisible (optionnel mais très utile)
#define CLR_RESET  "\x1b[0m"
#define CLR_INFO   "\x1b[32m" // Vert
#define CLR_WARN   "\x1b[33m" // Jaune
#define CLR_ERROR  "\x1b[31m" // Rouge

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/**
 * @brief Affiche un message de log avec horodatage et niveau de priorité
 * * @param level Le niveau (INFO, WARN, ERROR)
 * @param message Le message formaté (type printf)
 */
void lith_log(LogLevel level, const char *format, ...);

#endif