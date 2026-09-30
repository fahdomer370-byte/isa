CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Iinclude
PREFIX ?= /usr/local

BIN_DIR := $(PREFIX)/bin
SHARE_DIR := $(PREFIX)/share/isa
DOC_DIR := $(PREFIX)/share/doc/isa

.PHONY: all clean install

all: isa isac

isa: src/isa.o src/isa_core.o
	$(CC) $(CFLAGS) $^ -o $@

isac: src/isac.o src/isa_core.o
	$(CC) $(CFLAGS) $^ -o $@

src/%.o: src/%.c include/isa_core.h
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	install -Dm700 isa "$(BIN_DIR)/isa"
	install -Dm700 isac "$(BIN_DIR)/isac"
	install -Dm600 README.md "$(DOC_DIR)/README.md"
	install -Dm600 LICENSE "$(DOC_DIR)/LICENSE"
	install -Dm600 examples/hello.isa "$(SHARE_DIR)/examples/hello.isa"
	install -Dm600 examples/hello.isac "$(SHARE_DIR)/examples/hello.isac" 2>/dev/null || true

clean:
	rm -f isa isac src/*.o examples/hello.isac
