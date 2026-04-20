CC      ?= cc
CURL_CONFIG ?= curl-config
CURL_STATIC ?= 0
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=

CURL_CFLAGS := $(shell $(CURL_CONFIG) --cflags 2>/dev/null)

ifeq ($(CURL_STATIC),1)
CURL_LIBS := $(shell $(CURL_CONFIG) --static-libs 2>/dev/null)
else
CURL_LIBS := $(shell $(CURL_CONFIG) --libs 2>/dev/null)
endif

ifeq ($(strip $(CURL_LIBS)),)
CURL_LIBS := -lcurl
endif

CFLAGS  := -std=c11 -Wall -Wextra -O2 -Iinclude $(CURL_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS := $(CURL_LIBS) $(EXTRA_LDFLAGS)

ifeq ($(OS),Windows_NT)
TARGET := bad.exe
BIN    := ./bad.exe
else
TARGET := bad
BIN    := ./bad
endif

SRC := src/main.c \
       src/lexer.c \
       src/parser.c \
       src/json_helpers.c \
       src/http.c \
       src/vars.c \
       src/runtime.c

.PHONY: all clean run install

all: $(TARGET)

$(TARGET): $(SRC) include/bad.h include/bad_platform.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)
	@echo "  built: $(BIN)"

run: $(TARGET)
	$(BIN) examples/01-basics/quick_start_demo.bad --verbose

ifeq ($(OS),Windows_NT)
install: $(TARGET)
	@echo "  Windows build ready: $(TARGET)"
	@echo "  Add this folder to PATH or copy $(TARGET) into a PATH directory."
else
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/bad
	@echo "  installed to /usr/local/bin/bad"
endif

clean:
	rm -f bad bad.exe
