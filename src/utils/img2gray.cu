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

__global__ void rgb2gray(struct cuda_image * gray, const struct cuda_image * color)
{
  unsigned int w;
  unsigned int tid, i;

  w = gray->cu_width;
  tid = (unsigned int) (blockIdx.x * blockDim.x + threadIdx.x);
  i = tid / w;
  if (i < gray->cu_height) {
    unsigned int j, r, g, b;
    const struct turbo_rgb * rgb;

    j = tid % w;
    rgb = (const struct turbo_rgb *) color->cu_rows[i];
    rgb += j;
    r = (unsigned int) rgb->r;
    g = (unsigned int) rgb->g;
    b = (unsigned int) rgb->b;
    r = (r * 1254097 + g * 2462056 + b * 478151) >> 22;
    gray->cu_rows[i][j] = (r >= 256) ? 255 : (unsigned char) r;
  }
}

int main(int argc, char *argv[])
{
  char name[128];
  int i, numthreads;

  numthreads = 256;
  for (i = 1; i < argc; ++i) {
    int numblks;
    unsigned int w, h;
    struct turbo_jpeg * jpeg;
    struct cuda_image * cuimg, * gray;

    gray = nullptr;
    cuimg = nullptr;
    jpeg = turbo_jpeg_load(argv[i], TURBO_JPEG_RGB);
    if (jpeg == NULL)
      continue;

    cuimg = turbo_image_to_cuda(jpeg);
    if (cuimg == nullptr) {
      fprintf(stderr, "Error, failed to copy image to cuda device: %d\n", i);
      fflush(stderr);
      turbo_jpeg_free(jpeg);
      continue;
    }

    w = jpeg->tj_width;
    h = jpeg->tj_height;
    turbo_jpeg_free(jpeg);
    jpeg = nullptr;

    /* perform the translation of COLOR -> GRAY image */
    gray = cuda_image_new(w, h, TURBO_JPEG_GRAY, nullptr);
    if (gray == nullptr) {
      fprintf(stderr, "Error, failed to create gray CUDA image: %ux%u\n", w, h);
      fflush(stderr);
      cuda_image_free(cuimg);
      continue;
    }

    numblks = (int) (w * h);
    if ((numblks % numthreads) != 0)
      numblks = (numblks / numthreads) + 1;
    else
      numblks /= numthreads;
    rgb2gray<<<numblks, numthreads>>>(gray, cuimg);
    cudaDeviceSynchronize();

    cuda_image_free(cuimg);
    cuimg = nullptr;
    jpeg = turbo_image_from_cuda(gray);
    if (jpeg == nullptr) {
      fprintf(stderr, "Error, failed to copy image from cuda device: %d\n", i);
      fflush(stderr);
      cuda_image_free(gray);
      continue;
    }

    cuda_image_free(gray);
    snprintf(name, sizeof(name), "%03d.jpg", i);
    turbo_jpeg_save(jpeg, name, 98, 1);
    turbo_jpeg_free(jpeg);
  }
}
