#include "stdafx.h"

#include "base64.h"
#pragma warning(disable: 4365)

const char b64_encoding[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const unsigned char b64_decoding[128] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 62,  0,  0,  0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0
};



int b64_enc(void* in, unsigned size, char* out)
{
    unsigned char* it = (unsigned char*)in;
    unsigned i = 0;
    unsigned j = 0;
    unsigned k = 0;

    for (; i < (unsigned)(size / 3); ++i)
    {
        out[k + 0] = b64_encoding[((it[j + 0] & (~0x03)) >> 2)];
        out[k + 1] = b64_encoding[((it[j + 0] & 0x03) << 4) | ((it[j + 1] & (~0x0f)) >> 4)];
        out[k + 2] = b64_encoding[((it[j + 1] & 0x0f) << 2) | ((it[j + 2] & (~0x3f)) >> 6)];
        out[k + 3] = b64_encoding[  it[j + 2] & 0x3f];
        k += 4;
        j += 3;
    }

    i = size % 3;
    switch (i)
    {
    case 2:
        out[k + 0] = b64_encoding[((it[j + 0] & (~0x03)) >> 2)];
        out[k + 1] = b64_encoding[((it[j + 0] & 0x03)    << 4) | ((it[j + 1] & (~0x0f)) >> 4)];
        out[k + 2] = b64_encoding[((it[j + 1] & 0x0f)    << 2)];
        break;
    case 1:
        out[k + 0] = b64_encoding[((it[j + 0] & (~0x03)) >> 2)];
        out[k + 1] = b64_encoding[((it[j + 0] & 0x03)    << 4)];
        break;
    }
    k += i;
    out[++k] = '\0';

    return k - 1;
}

int b64_dec(const char* in, void* out)
{
    unsigned char* o = (unsigned char*)out;
    unsigned i = 0;
    unsigned j = 0;
    unsigned k = 0;
    unsigned len = strlen(in);

    for (; i < (unsigned)(len / 4); ++i)
    {

        o[k + 0] = (b64_decoding[in[j + 0]] << 2) | (((b64_decoding[in[j + 1]] & 0x30) >> 4));
        o[k + 1] = ((b64_decoding[in[j + 1]] & 0x0f) << 4) | ((b64_decoding[in[j + 2]] & 0x3C) >> 2);
        o[k + 2] = ((b64_decoding[in[j + 2]] & 0x03) << 6) | b64_decoding[in[j + 3]];
        k += 3;
        j += 4;
    }

    i = len % 4;
    switch (i)
    {
    case 3:
        o[k + 0] = (b64_decoding[in[j + 0]] << 2) | (((b64_decoding[in[j + 1]] & 0x30) >> 4));
        o[k + 1] = ((b64_decoding[in[j + 1]] & 0x0f) << 4) | ((b64_decoding[in[j + 2]] & 0x3C) >> 2);
        o[k + 2] = ((b64_decoding[in[j + 2]] & 0x03) << 6);
        break;
    case 2:
        o[k + 0] = (b64_decoding[in[j + 0]] << 2) | (((b64_decoding[in[j + 1]] & 0x30) >> 4));
        o[k + 1] = ((b64_decoding[in[j + 1]] & 0x0f) << 4);
        break;
    case 1:
        o[k + 0] = (b64_decoding[in[j + 0]] << 2);
        break;
    }
    k += i;
    return static_cast<int>(k - 1);
}
