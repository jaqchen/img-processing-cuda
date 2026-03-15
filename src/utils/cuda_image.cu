/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple image conversion between host and CUDA
 *
 * Licence: GPLv2
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "cuda_image.cuh"

struct cuda_image * turbo_image_to_cuda(const struct turbo_jpeg * tj)
{
  struct cuda_image * ci;
  unsigned char * imgptr;

  ci = nullptr;
  imgptr = nullptr;
  if (tj == nullptr || tj->tj_rows == nullptr)
    return ci;

  ci = cuda_image_new(tj->tj_width, tj->tj_height, tj->tj_color, &imgptr);
  if (ci == nullptr)
    return ci;

  cudaMemcpy(imgptr, tj->tj_buffer, (size_t) tj->tj_bufsize, cudaMemcpyHostToDevice);
  return ci;
}

__global__ void cuda_image_setup(struct cuda_image * ci,
  unsigned char * imgbuf, size_t bufsize, size_t rowsize,
  unsigned int width, unsigned int height, int cspace)
{
  size_t offset;
  unsigned char * cptr;
  unsigned int i, rsize;

  cptr = (unsigned char *) ci;
  offset = sizeof(*ci);
  if (offset & 0x7)
    offset = (offset & ~0x7) + 8;

  ci->cu_rows = (unsigned char **) (cptr + offset);
  ci->cu_buffer = imgbuf;
  ci->cu_width = width;
  ci->cu_height = height;
  ci->cu_rowsize = (unsigned int) rowsize;
  ci->cu_bufsize = (unsigned int) bufsize;
  ci->cu_color = cspace;

  rsize = (unsigned int) rowsize;
  for (i = 0; i < height; ++i)
    ci->cu_rows[i] = &imgbuf[i * rsize];
  ci->cu_rows[height] = nullptr;
}

struct cuda_image * cuda_image_new(unsigned int width,
  unsigned int height, int cspace, unsigned char ** cu_bufptr)
{
  size_t tsize;
  size_t rowlen, bufsize;
  struct cuda_image * ci;
  unsigned char * imgbuf;

  ci = nullptr;
  imgbuf = nullptr;
  if (width < TURBO_JPEG_MIN || width > TURBO_JPEG_MAX ||
    height < TURBO_JPEG_MIN || height > TURBO_JPEG_MAX) {
    fprintf(stderr, "Error, invalid cuda_image size: %ux%u.\n", width, height);
    fflush(stderr);
    return ci;
  }

  if (turbo_jpeg_checkcolor(cspace) < 0)
    return ci;

  /* allocate CUDA memory for image pixels */
  bufsize = (cspace == TURBO_JPEG_GRAY) ? 1 : 3;
  bufsize *= (size_t) width;
  rowlen = bufsize;
  bufsize *= (size_t) height;
  cudaMalloc(&imgbuf, bufsize);

  /* allocate CUDA memory for `cuda_image structure */
  tsize = sizeof(*ci) + 2 * sizeof(void *);
  tsize += (size_t) height * sizeof(void *) + sizeof(void *);
  cudaMalloc(&ci, tsize);

  /* setup the `cuda_image structure */
  cuda_image_setup<<<1, 1>>>(ci, imgbuf, bufsize, rowlen, width, height, cspace);
  cudaDeviceSynchronize();
  if (cu_bufptr != nullptr)
    *cu_bufptr = imgbuf;
  return ci;
}

__global__ void cuda_image_free1(struct cuda_image * ci)
{
  ci->cu_rows = nullptr;
  ci->cu_buffer = nullptr;
  ci->cu_width = 0;
  ci->cu_height = 0;
  ci->cu_rowsize = 0;
  ci->cu_bufsize = 0;
  ci->cu_color = 0;
}

void cuda_image_free(struct cuda_image * ci)
{
  struct cuda_image cm;
  unsigned char * imptr;

  if (ci == nullptr)
    return;

  cm.cu_buffer = nullptr;
  cudaMemcpy(&cm, ci, sizeof(cm), cudaMemcpyDeviceToHost);
  imptr = cm.cu_buffer;

  cuda_image_free1<<<1, 1>>>(ci);
  cudaDeviceSynchronize();
  cudaFree(ci);
  if (imptr != nullptr)
    cudaFree(imptr);
}

struct turbo_jpeg * turbo_image_from_cuda(struct cuda_image * ci)
{
  int color;
  struct cuda_image cm;
  unsigned char * imgptr;
  struct turbo_jpeg * tj;
  unsigned int w, h, bufsize;

  if (ci == nullptr)
    return nullptr;

  cm.cu_buffer = nullptr;
  cm.cu_width = 0;
  cm.cu_height = 0;
  cm.cu_bufsize = 0;
  cm.cu_color = -1;
  cudaMemcpy(&cm, ci, sizeof(cm), cudaMemcpyDeviceToHost);
  color = cm.cu_color;
  if (turbo_jpeg_checkcolor(color) < 0)
    return nullptr;

  w = cm.cu_width;
  h = cm.cu_height;
  if (w < TURBO_JPEG_MIN || w > TURBO_JPEG_MAX ||
    h < TURBO_JPEG_MIN || h > TURBO_JPEG_MAX) {
    fprintf(stderr, "Error, invalid `cuda_image size: %ux%u\n", w, h);
    fflush(stderr);
    return nullptr;
  }

  imgptr = cm.cu_buffer;
  bufsize = cm.cu_bufsize;
  if (imgptr == nullptr || bufsize == 0) {
    fprintf(stderr, "Error, invalid null `cuda_image buffer, size: %ux%u\n", w, h);
    fflush(stderr);
    return nullptr;
  }

  tj = turbo_jpeg_new(w, h, color);
  if (tj == nullptr)
    return nullptr;

  cudaMemcpy(tj->tj_buffer, imgptr, bufsize, cudaMemcpyDeviceToHost);
  return tj;
}
