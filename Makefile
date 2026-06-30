CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
INCLUDES = -Iinclude
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/lith

# Détection de l'OS pour les bibliothèques
ifeq ($(OS),Windows_NT)
	LIBS = -lws2_32
	TARGET := $(TARGET).exe
else
	LIBS = 
endif

# Détection automatique des fichiers sources à la racine et dans src/server/
SRC = $(wildcard src/*.c) $(wildcard src/server/*.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# Règle de compilation générique pour tous les fichiers .c
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Nettoyage portable et robuste des fichiers objets et des exécutables
clean:
	rm -f src/*.o src/server/*.o $(TARGET_DIR)/lith $(TARGET_DIR)/lith.exe