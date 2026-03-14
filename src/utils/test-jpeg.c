/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple turbo JPEG test
 *
 * Licence: GPLv2
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "turbo_jpeg.h"

int main(int argc, char *argv[])
{
  int w, h, i, j;
  struct turbo_rgb * rgb;
  struct turbo_jpeg * jpeg;

  w = h = 512;
  jpeg = turbo_jpeg_new(w, h, TURBO_JPEG_RGB);
  if (jpeg == NULL)
    return -1;

  for (i = 0; i < h; ++i) {
    rgb = (struct turbo_rgb *) jpeg->tj_rows[i];
    for (j = 0; j < w; ++j) {
      rgb->r = (unsigned char) j;
      rgb->g = (unsigned char) (j + j / 2);
      rgb->b = (unsigned char) (j << 1);
      rgb++;
    }
  }

  turbo_jpeg_save(jpeg, "test.jpg", 98, 1);
  turbo_jpeg_free(jpeg);
  return 0;
}
