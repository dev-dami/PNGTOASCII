#ifndef FIB_IMAGE_H
#define FIB_IMAGE_H

typedef struct {
    int width;
    int height;
    unsigned char *pixels;
} FibImage;

void fib_image_free(FibImage *image);
int fib_image_load(const char *path, FibImage *image);

#endif
