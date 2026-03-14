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

/*
 * image width or height should not be smaller than `TURBO_JPEG_MIN
 * image width or height should not be largar than `TURBO_JPEG_MAX
 */
#define TURBO_JPEG_MIN   12
#define TURBO_JPEG_MAX   65535

struct turbo_jpeg {
	unsigned char * tj_pixels; /* dynamically allocated memory holding the pixels data */
	int tj_width; /* width of the image in pixels */
	int tj_height; /* height of the image in pixels */
	int tj_isgray; /* non-zero: grayscale image; zero: RGB image */
};

struct turbo_jpeg * turbo_jpeg_new(int width, int height, isgray);

struct turbo_jpeg * turbo_jpeg_load(const char * filename);

int turbo_jpeg_save(struct turbo_jpeg * tj, const char * filename);

#ifdef __cplusplus
}
#endif
#endif
