CC ?= cc
CFLAGS ?= -Os
CPPFLAGS ?=
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CPPFLAGS += -Isrc
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections -s

PROGRAM = bin/lebtools
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean install uninstall

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	mkdir -p bin
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

install: $(PROGRAM)
	@set -e; \
	if test "$$(id -u)" = 0 && test -n "$$SUDO_UID" && test -n "$$SUDO_GID"; then \
		trap 'chown -R "$$SUDO_UID:$$SUDO_GID" build bin' EXIT; \
	fi; \
	install -d "$(DESTDIR)$(BINDIR)"; \
	install -m 755 $(PROGRAM) "$(DESTDIR)$(BINDIR)/lebtools"; \
	printf '%s\n' "Installed lebtools to $(DESTDIR)$(BINDIR)/lebtools"

uninstall:
	@rm -f "$(DESTDIR)$(BINDIR)/lebtools"
	@printf '%s\n' "Uninstalled lebtools from $(DESTDIR)$(BINDIR)/lebtools"

clean:
	rm -rf build bin

-include $(DEPENDENCIES)
