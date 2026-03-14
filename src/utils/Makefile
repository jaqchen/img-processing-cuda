# Simple Makefile to test `turbo_jpeg.h
MAKEFLAGS   += -r -R
CC          := gcc
CFLAGS      := -I. -Wall -fPIC -O2 -ggdb -fno-omit-frame-pointer
LDFLAGS     := -ljpeg
TARGETS     := test-jpeg test-encode

.PHONY: all clean
all: $(TARGETS)

test-encode: test-encode.c cjpeg.c djpeg.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

test-jpeg: test-jpeg.c cjpeg.c djpeg.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	rm -rf *.o $(TARGETS)
