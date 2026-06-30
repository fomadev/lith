# ==============================================================================
#  LITH Engine - Multi-platform Automated Build Pipeline
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread -MMD -MP
INCLUDES = -Iinclude
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/lith

# 1. Détection de l'OS et ajustement des bibliothèques systèmes
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    TARGET := $(TARGET).exe
    MKDIR = if not exist $(TARGET_DIR) mkdir $(TARGET_DIR)
    CLEAN_CMD = del /q /s src\*.o src\server\*.o src\*.d src\server\*.d 2>nul & del /q $(TARGET_DIR)\* 2>nul
else
    LIBS = 
    MKDIR = mkdir -p $(TARGET_DIR)
    CLEAN_CMD = rm -f src/*.o src/server/*.o src/*.d src/server/*.d $(TARGET_DIR)/*
endif

# 2. Détection automatique des sources et des objets
SRC = $(wildcard src/*.c) $(wildcard src/server/*.c)
OBJ = $(SRC:.c=.o)

# Génération des fichiers de dépendances (.d) par GCC
DEPS = $(SRC:.c=.d)

# 3. Règle principale
all: $(TARGET)

# 4. Édition de liens (génère l'exécutable dans bin/)
$(TARGET): $(OBJ)
	$(MKDIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# 5. Règle de compilation des fichiers objets (.c -> .o)
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 6. Inclusion des dépendances réelles générées par GCC
-include $(DEPS)

# 7. Nettoyage portable et robuste des artefacts de build
clean:
	$(CLEAN_CMD)

.PHONY: all clean