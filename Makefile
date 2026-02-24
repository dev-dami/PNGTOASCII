CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic
PKG_CONFIG ?= pkg-config
TARGET := fib
ASAN_TARGET := fib_asan
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g

PNG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS := $(shell $(PKG_CONFIG) --libs libpng 2>/dev/null)
JPG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libjpeg 2>/dev/null)
JPG_LIBS := $(shell $(PKG_CONFIG) --libs libjpeg 2>/dev/null)

.PHONY: all build test memcheck fixtures fetch-images demo clean

all: build

build: $(TARGET)

$(TARGET): fib.c
	$(CC) $(CFLAGS) $(PNG_CFLAGS) $(JPG_CFLAGS) $< -o $@ $(PNG_LIBS) $(JPG_LIBS)

test: build
	$(MAKE) -C tests run ROOT_DIR=..

memcheck:
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $(PNG_CFLAGS) $(JPG_CFLAGS) fib.c -o $(ASAN_TARGET) $(PNG_LIBS) $(JPG_LIBS)
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) -C tests run ROOT_DIR=.. BIN=../$(ASAN_TARGET)
	rm -f $(ASAN_TARGET)

fixtures:
	python3 tests/scripts/generate_fixtures.py

fetch-images:
	./scripts/download_test_images.sh

demo: build fixtures
	mkdir -p demo
	./$(TARGET) tests/fixtures/gradient.png 96 40 demo/gradient_ascii.txt
	./$(TARGET) tests/fixtures/stripes.png 48 12 demo/stripes_ascii.txt
	./$(TARGET) tests/fixtures/white.jpg 48 12 demo/white_jpg_ascii.txt
	./$(TARGET) tests/fixtures/checker.png 96 32 demo/checker_ascii.txt
	./$(TARGET) tests/fixtures/radial.png 90 40 demo/radial_ascii.txt
	@echo "Demo outputs written to demo/"

clean:
	rm -f $(TARGET)
	rm -rf demo tests/output
