/*
 * Created by yejq.jiaqiang@gmail.com
 *
 * Simple Turbo JPEG wrapper
 *
 * Licence: GPLv2
 */

#ifndef TURBO_JPEG_H
#define TURBO_JPEG_H 1
#ifdef __cplusplus
extern "C" {
#endif

struct turbo_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} __attribute__((__packed__));

/*
 * image width or height should not be smaller than `TURBO_JPEG_MIN
 * image width or height should not be largar than `TURBO_JPEG_MAX
 */
#define TURBO_JPEG_MIN   12
#define TURBO_JPEG_MAX   16384

/* supported colorspaces */
#define TURBO_JPEG_GRAY  1
#define TURBO_JPEG_RGB   2
#define TURBO_JPEG_YUV   3

struct turbo_jpeg {
	unsigned char ** tj_rows; /* point to each row of pixels raw data */
	unsigned char * tj_buffer; /* actual buffer for the underlying data */
	int tj_width; /* width of the image in pixels */
	int tj_height; /* height of the image in pixels */
	int tj_color; /* non-zero: grayscale image; zero: RGB image */
};

struct turbo_jpeg * turbo_jpeg_new(int width, int height, int colorspace);

void turbo_jpeg_free(struct turbo_jpeg * tj);

struct turbo_jpeg * turbo_jpeg_load(const char * filename, int forcegray);

int turbo_jpeg_save(struct turbo_jpeg * tj,
	const char * filename, int quality, int progressive);

int turbo_jpeg_checkcolor(int colorspace);

#ifdef __cplusplus
}
#endif
#endif
