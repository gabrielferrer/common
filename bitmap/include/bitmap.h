/*
	file: bitmap.h
	programming: Gabriel Ferrer
	date: 16-08-2012
*/

#ifndef BITMAP_H
#define BITMAP_H

/* Every bitmap consists of a collection of integers with format: 0x00RRGGBB
	where R, G and B are nibbles for red, green and blue channels respectively
*/
typedef struct bitmap_s {
	int width;
	int height;
	unsigned int* data;
} bitmap_t;

bitmap_t* BMP_LoadBitmap (const char* path, const char* name);

#endif
