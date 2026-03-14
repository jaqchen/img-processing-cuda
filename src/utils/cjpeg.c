/*
 * cjpeg.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * Modified 2003-2011 by Guido Vollbeding.
 * Lossless JPEG Modifications:
 * Copyright (C) 1999, Ken Murchison.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2010, 2013-2014, 2017, 2019-2022, 2024-2026,
 *           D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains a command-line user interface for the JPEG compressor.
 * It should work on any system with Unix- or MS-DOS-style command lines.
 *
 * Two different command line styles are permitted, depending on the
 * compile-time switch TWO_FILE_COMMANDLINE:
 *      cjpeg [options]  inputfile outputfile
 *      cjpeg [options]  [inputfile]
 * In the second style, output is always to standard output, which you'd
 * normally redirect to a file or pipe to some other program.  Input is
 * either from a named file or from standard input (typically redirected).
 * The second style is convenient on Unix but is unhelpful on systems that
 * don't support pipes.  Also, you MUST use the first style if your system
 * doesn't do binary I/O to stdin/stdout.
 * To simplify script writing, the "-outfile" switch is provided.  The syntax
 *      cjpeg [options]  -outfile outputfile  inputfile
 * works regardless of which command line style is used.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jpeglib.h>
#include "turbo_jpeg.h"

struct turbo_jpeg * turbo_jpeg_new(int width, int height, int isgray)
{
  size_t tsize;
  int row_size, i;
  struct turbo_jpeg * jpeg;
  unsigned char * imgbuf, * jptr;

  jpeg = NULL;
  if (width < TURBO_JPEG_MIN || width > TURBO_JPEG_MAX ||
    height < TURBO_JPEG_MIN || height > TURBO_JPEG_MAX) {
    fprintf(stderr, "Error, invalid image size for jpeg_new: %dx%d\n",
      width, height);
    fflush(stderr);
    return NULL;
  }

  if (isgray != 0)
    row_size = width; /* grayscale image */
  else
    row_size = width * 3; /* RGB image */

#if 0
  if (row_size & 0x3)
    row_size = (row_size & ~0x3) + 0x4;
#endif

  imgbuf = (unsigned char *) calloc((size_t) height, (size_t) row_size);
  if (imgbuf == NULL) {
    fprintf(stderr, "Error, failed to allocate memory for %s image: %dx%d\n",
      isgray ? "grayscale" : "RGB color", width, height);
    fflush(stderr);
    return NULL;
  }

  tsize = sizeof(*jpeg) + 2 * sizeof(void *);
  tsize += (size_t) (height * sizeof(void *) + sizeof(void *));
  jptr = (unsigned char *) calloc(1, tsize);
  if (jptr == NULL) {
    fprintf(stderr, "Error, failed to allocate memory for turbo jpeg: %zu\n", tsize);
    fflush(stderr);
    free(imgbuf);
    return NULL;
  }
  jpeg = (struct turbo_jpeg *) jptr;

  tsize = sizeof(*jpeg);
  if (tsize & 0x7)
    tsize = (tsize & ~0x7) + 8;
  jpeg->tj_rows    = (unsigned char **) (jptr + tsize);
  jpeg->tj_buffer  = imgbuf;
  jpeg->tj_width   = width;
  jpeg->tj_height  = height;
  jpeg->tj_isgray  = isgray;
  for (i = 0; i < height; ++i)
    jpeg->tj_rows[i] = &imgbuf[i * row_size];
  return jpeg;
}

void turbo_jpeg_free(struct turbo_jpeg * tj)
{
  if (tj == NULL)
    return;

  tj->tj_rows = NULL;
  if (tj->tj_buffer != NULL) {
    free(tj->tj_buffer);
    tj->tj_buffer = NULL;
  }
  tj->tj_width = tj->tj_height = 0;
  tj->tj_isgray = 0;
  free(tj);
}

static void my_emit_message(j_common_ptr cinfo, int msg_level)
{
  if (msg_level < 0) {
    /* Treat warning as fatal */
    cinfo->err->error_exit(cinfo);
  } else if (cinfo->err->trace_level >= msg_level) {
    cinfo->err->output_message(cinfo);
  }
}

int turbo_jpeg_save(struct turbo_jpeg * tj,
	const char * filename, int quality, int progressive)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *output_file = NULL;

  if (tj == NULL || tj->tj_rows == NULL) {
    fputs("Error, invalid turbo_jpeg encoding handle.\n", stderr);
    fflush(stderr);
    return -1;
  }

  if (tj->tj_width < TURBO_JPEG_MIN || tj->tj_width > TURBO_JPEG_MAX ||
    tj->tj_height < TURBO_JPEG_MIN || tj->tj_height > TURBO_JPEG_MAX) {
    fprintf(stderr, "Error, invalid image size: %dx%d\n", tj->tj_width, tj->tj_height);
    fflush(stderr);
    return -1;
  }

  if (filename == NULL || filename[0] == '\0') {
    fputs("Error, output file name not specified for jpeg_save.\n", stderr);
    fflush(stderr);
    return -1;
  }

  if (quality < 50 || quality > 100) {
    fprintf(stderr, "Error, invalid JPEG encoding quality: %d\n", quality);
    fflush(stderr);
    return -1;
  }

  output_file = fopen(filename, "wbe");
  if (output_file == NULL) {
    fprintf(stderr, "Error, jpeg_save can't create file %s\n", filename);
    fflush(stderr);
    return -1;
  }

  /* clear local variables to avoid any possible problem */
  memset(&jerr, 0, sizeof(jerr));
  memset(&cinfo, 0, sizeof(cinfo));

  /* Initialize the JPEG compression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  /* Add some application-specific error messages (from cderror.h) */

  /* Initialize JPEG parameters.
   * Much of this may be overridden later.
   * In particular, we don't yet know the input file's color space,
   * but we need to provide some value for jpeg_set_defaults() to work.
   */
  cinfo.data_precision = 8;
  cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
  cinfo.image_width = (JDIMENSION) tj->tj_width;
  cinfo.image_height = (JDIMENSION) tj->tj_height;
  jpeg_set_defaults(&cinfo);

  /* Scan command line to find file names.
   * It is convenient to use just one switch-parsing routine, but the switch
   * values read here are ignored; we will rescan the switches after opening
   * the input file.
   */
  cinfo.dct_method = JDCT_ISLOW;
  cinfo.input_components = tj->tj_isgray ? 1 : 3;
  jpeg_set_colorspace(&cinfo, tj->tj_isgray ? JCS_GRAYSCALE : JCS_RGB);
  jpeg_set_quality(&cinfo, quality, 0);

  jerr.emit_message = my_emit_message;
  /* Now that we know input colorspace, fix colorspace-dependent defaults */
  jpeg_default_colorspace(&cinfo);
  if (progressive != 0)
    jpeg_simple_progression(&cinfo);
  cinfo.err->trace_level = 0;

  jpeg_stdio_dest(&cinfo, output_file);
#ifdef ENTROPY_OPT_SUPPORTED
  cinfo.optimize_coding = TRUE;
#endif
  /* Start compressor */
  jpeg_start_compress(&cinfo, TRUE);

  /* Process data */
  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1];
    row_pointer[0] = tj->tj_rows[cinfo.next_scanline];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Finish compression and release memory */
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(output_file);

  /* All done. */
  if (jerr.num_warnings != 0) {
    fprintf(stdout, "Warning, encoding JPEG image has %ld warnings: %s\n",
      jerr.num_warnings, filename);
    fflush(stdout);
    return -1;
  }
  return 0;
}
