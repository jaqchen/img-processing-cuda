# Simple Makefile to test `turbo_jpeg.h
MAKEFLAGS   += -r -R
CC          := gcc
CFLAGS      := -I. -Wall -fPIC -O2 -ggdb -fno-omit-frame-pointer
LDFLAGS     := -ljpeg
TARGETS     := test-jpeg

.PHONY: all clean
all: $(TARGETS)

test-jpeg: test-jpeg.c cjpeg.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	rm -rf *.o $(TARGETS)
