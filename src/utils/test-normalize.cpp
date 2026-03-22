/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple RGB to grayscale image transformation in CUDA
 *
 * LICENSE: GPLv2
 */

#include <cstdio>
#include "cuda_image.cuh"

/*
 * JPEG images are mostly YCbCr encoded, so it is ridiculous to
 * decode a JPEG image to RGB image and then transform it back to
 * grayscale image. However, this is a simple test in CUDA.
 */

int main(int argc, char *argv[])
{
  int i;
  char name[128];

  for (i = 1; i < argc; ++i) {
    struct turbo_jpeg * jpeg;
    struct cuda_image * newimg;

    newimg = nullptr;
    jpeg = turbo_jpeg_load(argv[i], TURBO_JPEG_RGB);
    if (jpeg == NULL)
      continue;

    newimg = turbo_image_to_cuda(jpeg, i == 1 ? 1 : 128);
    if (newimg == nullptr) {
      fprintf(stderr, "Error, failed to copy image to cuda device: %d\n", i);
      fflush(stderr);
      turbo_jpeg_free(jpeg);
      continue;
    }

    turbo_jpeg_free(jpeg);
    jpeg = turbo_image_from_cuda(newimg);
    if (jpeg == nullptr) {
      fprintf(stderr, "Error, failed to copy image from cuda device: %d\n", i);
      fflush(stderr);
      cuda_image_free(newimg);
      continue;
    }

    cuda_image_free(newimg);
    snprintf(name, sizeof(name), "%03d.jpg", i);
    turbo_jpeg_save(jpeg, name, 98, 1);
    turbo_jpeg_free(jpeg);
    fprintf(stdout, "Image converted: %s => %s\n", argv[i], name);
    fflush(stdout);
  }
  return 0;
}
