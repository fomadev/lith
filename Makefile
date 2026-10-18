# ==============================================================================
#  LITH 1.0.0 - Makefile multiplateforme (Linux, macOS, Windows/MinGW)
#
#  make              compile bin/lith (ou bin/lith.exe)
#  make test         compile et lance les tests unitaires
#  make integration  lance les tests d'intégration (bash + curl)
#  make DEBUG=1      compilation de débogage (+ sanitizers hors Windows)
#  make STATIC=1     exécutable statique (Docker, Alpine)
#  make install      installe dans $(PREFIX)/bin (Linux/macOS)
#  make clean        supprime les fichiers générés
# ==============================================================================

CC       ?= gcc
ifeq ($(origin CC),default)
    CC = gcc
endif

PREFIX   ?= /usr/local
BIN_DIR   = bin
OBJ_DIR   = build

# CFLAGS / LDFLAGS restent libres pour l'utilisateur (make CFLAGS="-O3 -march=native").
CFLAGS   ?= -O2
LDFLAGS  ?=
LITH_CFLAGS  = -std=c11 -Wall -Wextra -pthread -MMD -MP -Iinclude
LITH_LDFLAGS =
LIBS      = -pthread

SRCS      = $(wildcard src/*.c)
OBJS      = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIB_OBJS  = $(filter-out $(OBJ_DIR)/main.o,$(OBJS))
DEPS      = $(OBJS:.o=.d)

ifeq ($(OS),Windows_NT)
    EXE       = .exe
    LITH_CFLAGS += -D_WIN32_WINNT=0x0601 -D__USE_MINGW_ANSI_STDIO=1
    LIBS     += -lws2_32
    WINDRES  ?= windres
    RES_OBJ   = $(OBJ_DIR)/resources.res
else
    EXE       =
    LITH_CFLAGS += -D_GNU_SOURCE
    RES_OBJ   =
endif

ifeq ($(DEBUG),1)
    LITH_CFLAGS += -O0 -g
    ifneq ($(OS),Windows_NT)
        LITH_CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
        LITH_LDFLAGS += -fsanitize=address,undefined
    endif
endif

ifeq ($(STATIC),1)
    LITH_LDFLAGS += -static
endif

TARGET      = $(BIN_DIR)/lith$(EXE)
TEST_TARGET = $(BIN_DIR)/unit_tests$(EXE)

.PHONY: all test integration install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJS) $(RES_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -o $@ $(OBJS) $(RES_OBJ) $(LDFLAGS) $(LITH_LDFLAGS) $(LIBS)

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -c $< -o $@

$(OBJ_DIR)/resources.res: resources.rc app_icon.ico
	@mkdir -p $(OBJ_DIR)
	$(WINDRES) $< -O coff -o $@

$(OBJ_DIR)/unit_tests.o: tests/unit_tests.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -c $< -o $@

$(TEST_TARGET): $(LIB_OBJS) $(OBJ_DIR)/unit_tests.o
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -o $@ $^ $(LDFLAGS) $(LITH_LDFLAGS) $(LIBS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

integration: $(TARGET)
	bash tests/integration.sh

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/lith

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/lith

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

-include $(DEPS) $(OBJ_DIR)/unit_tests.d
