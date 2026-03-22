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

__global__ void cuda_image_normalize(float * rowsptr,
  unsigned char * rawpix, unsigned int totlen, int offset)
{
  union {
    signed int ival;
    unsigned int uval;
  } v;
  unsigned int pid, id, stride;

  stride = (unsigned int) (blockDim.x * gridDim.x);
  pid = (unsigned int) (blockIdx.x * blockDim.x + threadIdx.x);
  for (id = pid; id < totlen; id += stride) {
    float * rowptr;
    unsigned char * pix;

    pix = rawpix + id;
    v.uval = (unsigned int) pix[0];
    v.ival = v.ival - offset;
    rowptr = rowsptr + id;
    rowptr[0] = (float) v.ival / 255.0f;
  }
}

__global__ void cuda_image_denorm(unsigned char * rowsptr,
  float * rawpix, unsigned int totlen, int offset)
{
  unsigned int pid, id, stride;

  stride = (unsigned int) (blockDim.x * gridDim.x);
  pid = (unsigned int) (blockIdx.x * blockDim.x + threadIdx.x);
  for (id = pid; id < totlen; id += stride) {
    signed int v;
    float * pix, fval;
    unsigned char * rowptr;

    pix = rawpix + id;
    fval = pix[0] * 255.0f + 0.5f;
    v = offset + (signed int) fval;

    rowptr = rowsptr + id;
    if (v >= 256)
      *rowptr = 255;
    else if (v < 0)
      *rowptr = 0;
    else
      *rowptr = (unsigned char) v;
  }
}

struct cuda_image * turbo_image_to_cuda(const struct turbo_jpeg * tj, int normalize)
{
  struct cuda_image * ci;
  unsigned char * imgptr, * imptr;

  ci = nullptr;
  imgptr = nullptr;
  if (tj == nullptr || tj->tj_buffer == nullptr || tj->tj_bufsize == 0)
    return ci;

  if (tj->tj_rowsize * tj->tj_height != tj->tj_bufsize) {
    fprintf(stderr, "Error, the image should not be padded, rowsize: %u, height: %u, bufsize: %u\n",
      tj->tj_rowsize, tj->tj_height, tj->tj_bufsize);
    fflush(stderr);
    return ci;
  }

  ci = cuda_image_new(tj->tj_width, tj->tj_height, normalize, tj->tj_color, &imgptr);
  if (ci == nullptr)
    return ci;

  if (normalize == 0) {
    cudaMemcpy(imgptr, tj->tj_buffer, (size_t) tj->tj_bufsize, cudaMemcpyHostToDevice);
    return ci;
  }

  imptr = nullptr;
  cudaMalloc(&imptr, tj->tj_bufsize);
  cudaMemcpy(imptr, tj->tj_buffer, (size_t) tj->tj_bufsize, cudaMemcpyHostToDevice);

  const unsigned int num_blocks = 256;
  const unsigned int num_threads = 256;
  cuda_image_normalize<<<num_blocks, num_threads>>>((float *) imgptr, imptr,
    tj->tj_bufsize, (normalize != 1) ? normalize : 0);
  cudaDeviceSynchronize();
  cudaFree(imptr);
  return ci;
}

static int cuda_image_setup(struct cuda_image * ci, size_t stsize,
  unsigned char * imgbuf, size_t bufsize, size_t rowsize,
  unsigned int width, unsigned int height, int normalize, int cspace)
{
  unsigned int i, rsize;
  struct cuda_image * cih;
  unsigned char * * prows;

  cih = (struct cuda_image *) calloc(0x1, stsize);
  if (cih == nullptr) {
    fprintf(stderr, "Error, failed to allocate memory for cuda_image: %zu\n", stsize);
    fflush(stderr);
    return -1;
  }

  cih->cu_rows = jpeg_image_rowsptr(ci, sizeof(*cih));
  cih->cu_buffer = imgbuf;
  cih->cu_width = width;
  cih->cu_height = height;
  cih->cu_rowsize = (unsigned int) rowsize;
  cih->cu_bufsize = (unsigned int) bufsize;
  cih->cu_normal = normalize;
  cih->cu_color = cspace;

  rsize = (unsigned int) rowsize;
  prows = jpeg_image_rowsptr(cih, sizeof(*cih));
  /* important: update again, the image rows-pointer array */
  for (i = 0; i < height; ++i)
    prows[i] = &imgbuf[i * rsize];
  prows[height] = nullptr;

  cudaMemcpy(ci, cih, stsize, cudaMemcpyHostToDevice);
  memset(cih, 0, sizeof(*cih));
  free(cih);
  return 0;
}

struct cuda_image * cuda_image_new(unsigned int width,
  unsigned int height, int normalize, int cspace, unsigned char ** cu_bufptr)
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
  if (normalize != 0)
    bufsize *= sizeof(float);
  rowlen = bufsize;
  bufsize *= (size_t) height;
  cudaMalloc(&imgbuf, bufsize);

  /* allocate CUDA memory for `cuda_image structure */
  tsize = sizeof(*ci) + 2 * sizeof(void *);
  tsize += (size_t) height * sizeof(void *) + sizeof(void *);
  cudaMalloc(&ci, tsize);

  /* setup the `cuda_image structure */
  ret = cuda_image_setup(ci, tsize, imgbuf, bufsize,
    rowlen, width, height, normalize, cspace);
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
  struct turbo_jpeg * tj;
  unsigned int w, h, bufsize;
  unsigned char * imgptr, * imptr;

  if (ci == nullptr)
    return nullptr;

  cm.cu_buffer = nullptr;
  cm.cu_width = 0;
  cm.cu_height = 0;
  cm.cu_bufsize = 0;
  cm.cu_normal = 0;
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

  if (cm.cu_normal == 0) {
    cudaMemcpy(tj->tj_buffer, imgptr, bufsize, cudaMemcpyDeviceToHost);
    return tj;
  }

  imptr = nullptr;
  cudaMalloc(&imptr, bufsize);
  const unsigned int num_blocks = 256;
  const unsigned int num_threads = 256;
  cuda_image_denorm<<<num_blocks, num_threads>>>(imptr, (float *) imgptr,
    tj->tj_bufsize, (cm.cu_normal != 1) ? cm.cu_normal : 0);
  cudaDeviceSynchronize();
  cudaMemcpy(tj->tj_buffer, imptr, tj->tj_bufsize, cudaMemcpyDeviceToHost);
  cudaFree(imptr);
  return tj;
}
