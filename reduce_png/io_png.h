#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <png.h>
#include <assert.h>

#ifndef EXTERN
#define EXTERN extern 
#endif

EXTERN int width, height;
EXTERN png_byte color_type;
EXTERN png_byte bit_depth;
EXTERN png_bytep *row_pointers;
EXTERN int **weights;

void read_png_file(char *filename);
void write_png_file(char *filename);