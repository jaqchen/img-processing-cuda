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

static int cuda_image_setup(struct cuda_image * ci, size_t stsize,
  unsigned char * imgbuf, size_t bufsize, size_t rowsize,
  unsigned int width, unsigned int height, int cspace)
{
  size_t offset;
  unsigned char * cptr;
  unsigned int i, rsize;
  struct cuda_image * cih;

  cih = (struct cuda_image *) calloc(0x1, stsize);
  if (cih == nullptr) {
    fprintf(stderr, "Error, failed to allocate memory for cuda_image: %zu\n", stsize);
    fflush(stderr);
    return -1;
  }

  cptr = (unsigned char *) cih;
  offset = sizeof(*cih);
  if (offset & 0x7)
    offset = (offset & ~0x7) + 8;

  cih->cu_rows = (unsigned char **) (cptr + offset);
  cih->cu_buffer = imgbuf;
  cih->cu_width = width;
  cih->cu_height = height;
  cih->cu_rowsize = (unsigned int) rowsize;
  cih->cu_bufsize = (unsigned int) bufsize;
  cih->cu_color = cspace;

  rsize = (unsigned int) rowsize;
  for (i = 0; i < height; ++i)
    cih->cu_rows[i] = &imgbuf[i * rsize];
  cih->cu_rows[height] = nullptr;

  /* important: update again, the image rows-pointer: */
  cptr = (unsigned char *) ci;
  cih->cu_rows = (unsigned char **) (cptr + offset);

  cudaMemcpy(ci, cih, stsize, cudaMemcpyHostToDevice);
  memset(cih, 0, sizeof(*cih));
  free(cih);
  return 0;
}

struct cuda_image * cuda_image_new(unsigned int width,
  unsigned int height, int cspace, unsigned char ** cu_bufptr)
{
  int ret;
  struct cuda_image * ci;
  unsigned char * imgbuf;
  size_t tsize, rowlen, bufsize;

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
  ret = cuda_image_setup(ci, tsize, imgbuf, bufsize,
    rowlen, width, height, cspace);
  if (ret < 0) {
    cudaFree(ci);
    cudaFree(imgbuf);
    return nullptr;
  }
  if (cu_bufptr != nullptr)
    *cu_bufptr = imgbuf;
  return ci;
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

  memset(&cm, 0, sizeof(cm));
  cudaMemcpy(ci, &cm, sizeof(cm), cudaMemcpyHostToDevice);
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
