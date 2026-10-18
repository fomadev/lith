# ==============================================================================
#  LITH 1.0.0 - Makefile multiplateforme (Linux, macOS, Windows/MinGW)
#
#  make              compile bin/lith (ou bin/lith.exe)
#  make test         compile et lance les tests unitaires
#  make integration  lance les tests d'intégration (bash + curl)
#  make DEBUG=1      compilation de débogage (+ sanitizers hors Windows)
#  make STATIC=1     exécutable statique (Docker, Alpine)
#  make TLS=1        ajoute HTTPS (module Rust tls/ basé sur rustls ; demande cargo)
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

# ---- HTTPS optionnel : bibliothèque statique Rust (tls/) liée au cœur C ----
# Sans TLS=1, rien de tout cela n'intervient : cœur 100 % C, aucune dépendance.
TLS ?= 0
ifeq ($(TLS),1)
    CARGO ?= cargo
    ifeq ($(OS),Windows_NT)
        # gcc/MinGW ne sait lier que la cible Rust GNU : rustup target add x86_64-pc-windows-gnu
        RUST_TARGET  ?= x86_64-pc-windows-gnu
        TLS_SYS_LIBS ?= -lbcrypt -ladvapi32 -lkernel32 -lntdll -luserenv -lws2_32 -ldbghelp
    else ifeq ($(shell uname -s),Darwin)
        TLS_SYS_LIBS ?= -framework CoreFoundation -framework Security -liconv -lc -lm
        TLS_STRIP     = -Wl,-dead_strip
    else
        TLS_SYS_LIBS ?= -ldl -lm -lrt
    endif
    # La bibliothèque standard Rust embarque ses symboles de débogage (~3,5 Mo) : on les retire.
    ifneq ($(DEBUG),1)
        TLS_STRIP    ?= -Wl,--gc-sections -s
        LITH_LDFLAGS += $(TLS_STRIP)
    endif
    RUST_TARGET_DIR = tls/target/$(if $(RUST_TARGET),$(RUST_TARGET)/)release
    TLS_LIB      = $(RUST_TARGET_DIR)/liblith_tls.a
    OBJ_DIR      = build/tls
    LITH_CFLAGS += -DLITH_TLS=1 -Itls
    LIBS        := $(TLS_LIB) $(TLS_SYS_LIBS) $(LIBS)
else
    TLS_LIB      =
endif

# Changer d'avis (TLS=1 <-> sans TLS) doit refaire l'édition de liens de bin/lith.
VARIANT_STAMP = build/.variant-tls$(TLS)
$(shell mkdir -p build && [ -f $(VARIANT_STAMP) ] || { rm -f build/.variant-tls*; touch $(VARIANT_STAMP); })

TARGET      = $(BIN_DIR)/lith$(EXE)
TEST_TARGET = $(BIN_DIR)/unit_tests$(EXE)

.PHONY: all test integration install uninstall clean distclean

all: $(TARGET)

$(TARGET): $(OBJS) $(RES_OBJ) $(TLS_LIB) $(VARIANT_STAMP)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -o $@ $(OBJS) $(RES_OBJ) $(LDFLAGS) $(LITH_LDFLAGS) $(LIBS)

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -c $< -o $@

$(OBJ_DIR)/resources.res: resources.rc app_icon.ico
	@mkdir -p $(OBJ_DIR)
	$(WINDRES) $< -O coff -o $@

# Module HTTPS (Rust). --locked : versions figées par tls/Cargo.lock.
$(TLS_LIB): tls/Cargo.toml tls/Cargo.lock tls/src/lib.rs
	cd tls && $(CARGO) build --release --locked $(if $(RUST_TARGET),--target $(RUST_TARGET))
	@touch $@

$(OBJ_DIR)/unit_tests.o: tests/unit_tests.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -c $< -o $@

$(TEST_TARGET): $(LIB_OBJS) $(OBJ_DIR)/unit_tests.o $(TLS_LIB) $(VARIANT_STAMP)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LITH_CFLAGS) -o $@ $(LIB_OBJS) $(OBJ_DIR)/unit_tests.o $(LDFLAGS) $(LITH_LDFLAGS) $(LIBS)

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
	rm -rf build $(BIN_DIR)

# Supprime aussi la compilation Rust du module HTTPS (longue à refaire).
distclean: clean
	rm -rf tls/target

-include $(DEPS) $(OBJ_DIR)/unit_tests.d
