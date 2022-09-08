#pragma once

#include <gdiplus.h>
#include "AtexReader.h"

union RGBA
{
	unsigned char c[4];
	struct { unsigned char r, g, b, a; };
	unsigned int dw;
};
bool ProcessImageFile(unsigned char* img, int size, Gdiplus::Bitmap* image);



