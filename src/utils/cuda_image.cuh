/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple Image structure for CUDA
 *
 * LICENSE: GPLv2
 */

#ifndef CUDA_IMAGE_H
#define CUDA_IMAGE_H 1

/* pure definitions in host C */
#include "turbo_jpeg.h"
#include "cuda_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the structure always resides in GPU memory */
struct cuda_image {
  unsigned char ** cu_rows;
  unsigned char * cu_buffer;
  unsigned int cu_width;
  unsigned int cu_height;
  unsigned int cu_rowsize;
  unsigned int cu_bufsize;
  int cu_color;
};

struct cuda_image * turbo_image_to_cuda(const struct turbo_jpeg * tj);

struct cuda_image * cuda_image_new(unsigned int width,
  unsigned int height, int colorspace, unsigned char ** cu_bufptr);

void cuda_image_free(struct cuda_image * ci);

struct turbo_jpeg * turbo_image_from_cuda(struct cuda_image * ci);

#ifdef __cplusplus
}
#endif
#endif
