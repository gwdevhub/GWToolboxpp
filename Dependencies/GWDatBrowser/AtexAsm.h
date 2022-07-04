#pragma once

struct SImageDescriptor {
    int xres, yres;
    unsigned char *Data;
    int a;
    int b;
    unsigned char *image;
    int imageformat;
    int c;
};

int DecompressAtex( int a, int b, int imageformat, int d, int e, int f, int g );
void AtexDecompress( unsigned int *input, unsigned int unknown, unsigned int imageformat, SImageDescriptor ImageDescriptor, unsigned int *output );
