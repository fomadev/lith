# ==============================================================================
#  LITH Engine - Multi-platform Automated Build Pipeline
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
INCLUDES = -Iinclude
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/lith

# 1. Détection de l'OS et ajustement des bibliothèques systèmes
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    TARGET := $(TARGET).exe
    MKDIR = if not exist $(TARGET_DIR) mkdir $(TARGET_DIR)
else
    LIBS = 
    MKDIR = mkdir -p $(TARGET_DIR)
endif

# 2. Détection automatique des sources et des fichiers d'en-têtes
SRC = $(wildcard src/*.c) $(wildcard src/server/*.c)
OBJ = $(SRC:.c=.o)
DEPS = $(wildcard include/*.h)

# 3. Règle principale
all: $(TARGET)

# 4. Édition de liens (génère l'exécutable dans bin/)
$(TARGET): $(OBJ)
	$(MKDIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# 5. Règle de compilation des fichiers objets (.c -> .o)
# Ajout de $(DEPS) pour forcer la recompilation si un fichier .h change
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 6. Nettoyage portable et robuste des artefacts de build
clean:
	rm -f src/*.o src/server/*.o $(TARGET_DIR)/lith $(TARGET_DIR)/lith.exe

.PHONY: all clean