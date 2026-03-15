/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple turbo JPEG encoding test
 *
 * Licence: GPLv2
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "turbo_jpeg.h"

int main(int argc, char *argv[])
{
  const char * img;
  struct turbo_jpeg * jpeg;

  img = (argc >= 2) ? argv[1] : NULL;
  if (img == NULL) {
    fputs("Error, no jpeg file specifed.\n", stderr);
    fflush(stderr);
    return 1;
  }

  jpeg = turbo_jpeg_load(img, TURBO_JPEG_YUV);
  if (jpeg == NULL)
    return 2;

  turbo_jpeg_save(jpeg, "test.jpg", 98, 1);
  turbo_jpeg_free(jpeg);
  return 0;
}
