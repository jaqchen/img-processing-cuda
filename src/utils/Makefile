# Simple Makefile to test `turbo_jpeg.h
MAKEFLAGS   += -r -R
CC          := gcc
CFLAGS      := -I. -Wall -fPIC -O2 -ggdb -fno-omit-frame-pointer
LDFLAGS     := -ljpeg
TARGETS     := test-jpeg test-encode img2gray

COMPILER_OPTS  := -forward-unknown-to-host-compiler -Wno-deprecated-gpu-targets \
	--extended-lambda -Xcompiler=-fPIC
ifneq ("$(wildcard /usr/local/cuda-12.8)","")
COMCAP_OPTS    := "--generate-code=arch=compute_86,code=[compute_86,sm_86]" --threads 0
else
COMCAP_OPTS    := "--generate-code=arch=compute_62,code=[compute_62,sm_62]"
endif

.PHONY: all clean
all: $(TARGETS)

test-encode: test-encode.o cjpeg.o djpeg.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

test-jpeg: test-jpeg.o cjpeg.o djpeg.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

img2gray: img2gray.o cjpeg.o djpeg.o cuda_image.o
	nvcc -ccbin g++ -I. $(COMPILER_OPTS) -m64 $(COMCAP_OPTS) -o $@ $^ $(LDFLAGS)

%.o: %.cu cuda_image.cuh turbo_jpeg.h
	nvcc -ccbin g++ -I. $(COMPILER_OPTS) -m64 --use_fast_math $(COMCAP_OPTS) -o $@ -c $<

%.o: %.c turbo_jpeg.h
	gcc -I. $(CFLAGS) -O2 -fPIC -o $@ -c $<

clean:
	rm -rf *.o $(TARGETS)
