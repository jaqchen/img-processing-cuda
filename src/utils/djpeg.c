/*
 * djpeg.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * Modified 2013-2019 by Guido Vollbeding.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2010-2011, 2013-2017, 2019-2020, 2022-2024, D. R. Commander.
 * Copyright (C) 2015, Google, Inc.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains a command-line user interface for the JPEG decompressor.
 * It should work on any system with Unix- or MS-DOS-style command lines.
 *
 * Two different command line styles are permitted, depending on the
 * compile-time switch TWO_FILE_COMMANDLINE:
 *      djpeg [options]  inputfile outputfile
 *      djpeg [options]  [inputfile]
 * In the second style, output is always to standard output, which you'd
 * normally redirect to a file or pipe to some other program.  Input is
 * either from a named file or from standard input (typically redirected).
 * The second style is convenient on Unix but is unhelpful on systems that
 * don't support pipes.  Also, you MUST use the first style if your system
 * doesn't do binary I/O to stdin/stdout.
 * To simplify script writing, the "-outfile" switch is provided.  The syntax
 *      djpeg [options]  -outfile outputfile  inputfile
 * works regardless of which command line style is used.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "jpeglib.h"
#include "turbo_jpeg.h"

#include <ctype.h>              /* to declare isprint() */

/*
 * Marker processor for COM and interesting APPn markers.
 * This replaces the library's built-in processor, which just skips the marker.
 * We want to print out the marker as text, to the extent possible.
 * Note this code relies on a non-suspending data source.
 */

static unsigned int jpeg_getc(j_decompress_ptr cinfo)
/* Read next byte */
{
  struct jpeg_source_mgr *datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (!(*datasrc->fill_input_buffer)(cinfo)) {
      /* ERREXIT(cinfo, JERR_CANT_SUSPEND); */
      fputs("Error, JPEG decoding cannot suspend!\n", stderr);
      fflush(stderr);
    }
  }
  datasrc->bytes_in_buffer--;
  return *datasrc->next_input_byte++;
}

static boolean print_text_marker(j_decompress_ptr cinfo)
{
  boolean traceit = (cinfo->err->trace_level >= 1);
  long length;
  unsigned int ch;
  unsigned int lastch = 0;

  length = jpeg_getc(cinfo) << 8;
  length += jpeg_getc(cinfo);
  length -= 2;                  /* discount the length word itself */

  if (traceit) {
    if (cinfo->unread_marker == JPEG_COM)
      fprintf(stderr, "Comment, length %ld:\n", (long)length);
    else                        /* assume it is an APPn otherwise */
      fprintf(stderr, "APP%d, length %ld:\n",
              cinfo->unread_marker - JPEG_APP0, (long)length);
  }

  while (--length >= 0) {
    ch = jpeg_getc(cinfo);
    if (traceit) {
      /* Emit the character in a readable form.
       * Nonprintables are converted to \nnn form,
       * while \ is converted to \\.
       * Newlines in CR, CR/LF, or LF form will be printed as one newline.
       */
      if (ch == '\r') {
        fprintf(stderr, "\n");
      } else if (ch == '\n') {
        if (lastch != '\r')
          fprintf(stderr, "\n");
      } else if (ch == '\\') {
        fprintf(stderr, "\\\\");
      } else if (isprint(ch)) {
        putc(ch, stderr);
      } else {
        fprintf(stderr, "\\%03o", ch);
      }
      lastch = ch;
    }
  }

  if (traceit)
    fprintf(stderr, "\n");

  return TRUE;
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

int turbo_jpeg_checkcolor(int colorspace)
{
  if (colorspace == TURBO_JPEG_GRAY)
    return JCS_GRAYSCALE;
  if (colorspace == TURBO_JPEG_RGB)
    return JCS_RGB;
  if (colorspace == TURBO_JPEG_YUV)
    return JCS_YCbCr;
  fprintf(stderr, "Error, unsupported colorspace specified: %d\n", colorspace);
  fflush(stderr);
  return JCS_UNKNOWN;
}

struct turbo_jpeg * turbo_jpeg_load(const char * filename, int forcegray)
{
  int colorspace;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE * input_file = NULL;
  struct turbo_jpeg * tj = NULL;

  colorspace = 0;
  memset(&jerr, 0, sizeof(jerr));
  memset(&cinfo, 0, sizeof(cinfo));

  /* Initialize the JPEG decompression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  /* Insert custom marker processor for COM and APP12.
   * APP12 is used by some digital camera makers for textual info,
   * so we provide the ability to display it as text.
   * If you like, additional APPn marker types can be selected for display,
   * but don't try to override APP0 or APP14 this way (see libjpeg.txt).
   */
  jpeg_set_marker_processor(&cinfo, JPEG_COM, print_text_marker);
  jpeg_set_marker_processor(&cinfo, JPEG_APP0 + 12, print_text_marker);

  /* Scan command line to find file names. */
  /* It is convenient to use just one switch-parsing routine, but the switch
   * values read here are ignored; we will rescan the switches after opening
   * the input file.
   * (Exception: tracing level set here controls verbosity for COM markers
   * found during jpeg_read_header...)
   */

  /* Open the input file. */
  input_file = fopen(filename, "rbe");
  if (input_file == NULL) {
    fprintf(stderr, "Error, failed to open JPEG file %s\n", filename);
    fflush(stderr);
    return NULL;
  }

  /* Specify data source for decompression */
  jpeg_stdio_src(&cinfo, input_file);

  /* Read file header, set default decompression parameters */
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    fprintf(stderr, "Error, failed to read JPEG header information: %s\n", filename);
    fflush(stderr);
err0:
    fclose(input_file);
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }

  if (cinfo.jpeg_color_space == JCS_UNKNOWN) {
    fprintf(stderr, "Error, invalid unknown colorspace for JPEG image '%s'\n", filename);
    fflush(stderr);
    goto err0;
  }

  if (forcegray != 0 || cinfo.jpeg_color_space == JCS_GRAYSCALE) {
    colorspace = TURBO_JPEG_GRAY;
    cinfo.out_color_space = JCS_GRAYSCALE;
  } else if (cinfo.jpeg_color_space == JCS_YCbCr || cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK) {
    colorspace = TURBO_JPEG_YUV;
    cinfo.out_color_space = JCS_YCbCr;
  } else {
    colorspace = TURBO_JPEG_RGB;
    cinfo.out_color_space = JCS_RGB;
  }
  jerr.emit_message = my_emit_message;

  if (cinfo.data_precision != 8) {
    fprintf(stderr, "Error, invalid data_precision for %s: %d\n", filename, cinfo.data_precision);
    fflush(stderr);
    goto err0;
  }

  if (cinfo.image_width < TURBO_JPEG_MIN || cinfo.image_width > TURBO_JPEG_MAX ||
    cinfo.image_height < TURBO_JPEG_MIN || cinfo.image_height > TURBO_JPEG_MAX) {
    fprintf(stderr, "Error, invalid image size for %s: %dx%d\n",
      filename, cinfo.image_width, cinfo.image_height);
    fflush(stderr);
    goto err0;
  }

  tj = turbo_jpeg_new(cinfo.image_width, cinfo.image_height, colorspace);
  if (tj == NULL) {
    fprintf(stderr, "Error, failed to create INMEMORY jpeg handle, size: %dx%d\n",
      cinfo.image_width, cinfo.image_height);
    fflush(stderr);
    goto err0;
  }

  /* Start decompressor */
  if (jpeg_start_decompress(&cinfo) == 0) {
    fprintf(stderr, "Error, failed to start decompression for %s\n", filename);
    fflush(stderr);
    turbo_jpeg_free(tj);
    tj = NULL;
    goto err0;
  }

  /* Process data */
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, &tj->tj_rows[cinfo.output_scanline],
      cinfo.output_height - cinfo.output_scanline);
  }

  /* Hack: count final pass as done in case finish_output does an extra pass.
   * The library won't have updated completed_passes.
   */
  /* Finish decompression and release memory.
   * I must do it in this order because output module has allocated memory
   * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
   */
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  /* Close files, if we opened them */
  fclose(input_file);

  /* All done. */
  if (jerr.num_warnings != 0) {
    fprintf(stderr, "Error, JPEG image '%s' decode has %ld warnings.\n", filename, jerr.num_warnings);
    fflush(stderr);
    turbo_jpeg_free(tj);
    return NULL;
  }
  return tj;
}
