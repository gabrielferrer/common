/*
	file: bitmap.c
	programming: Gabriel Ferrer
	date: 16-08-2012
*/

#include <stdio.h>
#include <stdlib.h>
#include "bitmap.h"

#define SIGNATURE_SIZE 2
#define MAXFILENAME 2048

/* Bitmap file header */
typedef struct bmpfileheader_s {
	char signature[SIGNATURE_SIZE];	// Signature. Must be 0x4d42
	int size;			// BMP file size in bytes
	int reserved;		// Reserved. Must be zero
	int offset;			// Start of image data in bytes
} bmpfileheader_t;

/* Bitmap info header */
typedef struct bmpinfoheader_s {
	int headersize;		// Header structure size. Must be 40
	int width;			// BMP width in pixels
	int height;			// BMP height in pixels
	short planes;		// Number of planes
	short bpp;			// Bits per pixel
	int ctype;			// Compression type (0=none, 1=RLE-8, 2=RLE-4)
	int datasize;		// Image data size in bytes
	int hres;			// Horizontal resolution in pixels per meter
	int vres;			// Vertical resolution in pixels per meter
	int colors;			// Image colors or zero
	int important;		// Image important colors or zero
} bmpinfoheader_t;

bitmap_t* BMP_LoadData (char* filename) {
	FILE* file;
	bmpfileheader_t bfh;
	bmpinfoheader_t bih;
	int row, inc, height, i, j, bpl, base;
	unsigned char r, g, b;
	bitmap_t* bitmap;

	if ((bitmap = (bitmap_t*) malloc (sizeof (bitmap_t))) == NULL) {
		return NULL;
	}

	if ((file = fopen (filename, "rb")) == NULL) {
		return NULL;
	}

	fread (&bfh.signature, SIGNATURE_SIZE, 1, file);

	if (bfh.signature[0] != 'B' || bfh.signature[1] != 'M') {
		return NULL;
	}

	fread (&bfh.size, sizeof (int), 1, file);
	fread (&bfh.reserved, sizeof (int), 1, file);
	fread (&bfh.offset, sizeof (int), 1, file);	
	fread (&bih.headersize, sizeof (int), 1, file);
	fread (&bih.width, sizeof (int), 1, file);
	fread (&bih.height, sizeof (int), 1, file);
	fread (&bih.planes, sizeof (short), 1, file);
	fread (&bih.bpp, sizeof (short), 1, file);
	fread (&bih.ctype, sizeof (int), 1, file);
	fread (&bih.datasize, sizeof (int), 1, file);
	fread (&bih.hres, sizeof (int), 1, file);
	fread (&bih.vres, sizeof (int), 1, file);
	fread (&bih.colors, sizeof (int), 1, file);
	fread (&bih.important, sizeof (int), 1, file);

	/* Height can be negative if bitmap is top-down */
	height = abs (bih.height);

	if ((bitmap->data = (unsigned int*) malloc (bih.width * height * sizeof (unsigned int))) == NULL) {
		return NULL;
	}

	/* Bitmap's bytes per line */
	bpl = bih.width * (bih.bpp >> 3) + 3 & ~3;
	/* Set texture width and height */
	bitmap->width = bih.width;
	bitmap->height = height;

	/* If height < 0 bitmap is top-down else bitmap is bottom-up */
	if (bih.height < 0) {
		row = 0;
		inc = 1;
	} else {
		row = height - 1;
		inc = -1;
	}

	for (i = 0, base = 0; i < height; i++, row += inc, base += bih.width) {
		fseek (file, row * bpl + bfh.offset, SEEK_SET);

		for (j = 0; j < bih.width; j++) {
			fread (&r, 1, 1, file);
			fread (&g, 1, 1, file);
			fread (&b, 1, 1, file);

			bitmap->data[base + j] =
				(unsigned int) r << 16 & 0x00ff0000 |
				(unsigned int) g << 8 & 0x0000ff00 |
				(unsigned int) b & 0x000000ff;
		}
	}

	return bitmap;
}

bitmap_t* BMP_LoadBitmap (const char* path, const char* name) {
	char filename[MAXFILENAME];
	int i;

	sprintf (filename, "%s/%s.bmp", path, name);

	return BMP_LoadData (filename);
}

