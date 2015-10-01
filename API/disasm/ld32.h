/*
x86 Length Disassembler.
Copyright (C) 2013 Byron Platt

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LD32_H__
#define __LD32_H__

/* implemented tables */
#define PREFIX_T    1
#define MODRM2_T    2
#define MODRM_T     4
#define DATA1_T     8
#define DATA2_T     16
#define DATA66_T    32

/* configure tables */
#ifndef USE_T
#define USE_T       (MODRM2_T|MODRM_T|DATA1_T|DATA66_T)
#endif

/* length_disasm */
unsigned int length_disasm(void* opcode0);

/* table macros */
#ifdef USE_T
#define BITMASK32(                                                             \
    b00,b01,b02,b03,b04,b05,b06,b07,                                           \
    b08,b09,b0a,b0b,b0c,b0d,b0e,b0f,                                           \
    b10,b11,b12,b13,b14,b15,b16,b17,                                           \
    b18,b19,b1a,b1b,b1c,b1d,b1e,b1f                                            \
) (                                                                            \
    (b00<<0x00)|(b01<<0x01)|(b02<<0x02)|(b03<<0x03)|                           \
    (b04<<0x04)|(b05<<0x05)|(b06<<0x06)|(b07<<0x07)|                           \
    (b08<<0x08)|(b09<<0x09)|(b0a<<0x0a)|(b0b<<0x0b)|                           \
    (b0c<<0x0c)|(b0d<<0x0d)|(b0e<<0x0e)|(b0f<<0x0f)|                           \
    (b10<<0x10)|(b11<<0x11)|(b12<<0x12)|(b13<<0x13)|                           \
    (b14<<0x14)|(b15<<0x15)|(b16<<0x16)|(b17<<0x17)|                           \
    (b18<<0x18)|(b19<<0x19)|(b1a<<0x1a)|(b1b<<0x1b)|                           \
    (b1c<<0x1c)|(b1d<<0x1d)|(b1e<<0x1e)|(b1f<<0x1f)                            \
)
#define CHECK_TABLE(t, v)   ((t[(v)>>5]>>((v)&0x1f))&1)
#endif

/* CHECK_PREFIX */
#if defined(USE_T) && (USE_T & PREFIX_T)
const static unsigned int prefix_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 0 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 1 */
    BITMASK32(0,0,0,0,0,0,1,0, 0,0,0,0,0,0,1,0,  /* 2 */
              0,0,0,0,0,0,1,0, 0,0,0,0,0,0,1,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,  /* 6 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 7 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 8 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 9 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* a */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* b */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* c */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* d */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* e */
              1,0,1,1,0,0,0,0, 0,0,0,0,0,0,0,0)  /* f */
};
#define CHECK_PREFIX(v) CHECK_TABLE(prefix_t, v)
#else
#define CHECK_PREFIX(v)                                                        \
    (((v)&0xe7)==0x26||((v)&0xfc)==0x64||(v)==0xf0||(v)==0xf2||(v)==0xf3)
#endif

/* CHECK_PREFIX_66 */
#define CHECK_PREFIX_66(v)  ((v)==0x66)

/* CHECK_PREFIX_67 */
#define CHECK_PREFIX_67(v)  ((v)==0x67)

/* CHECK_0F */
#define CHECK_0F(v)         ((v)==0x0f)

/* CHECK_MODRM2 */
#if defined(USE_T) && (USE_T & MODRM2_T)
const static unsigned int modrm2_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 0 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 1 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 2 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 6 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 7 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 8 */
              1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1), /* 9 */
    BITMASK32(0,0,0,1,1,1,0,0, 0,0,0,1,1,1,0,1,  /* a */
              1,1,1,1,1,1,1,1, 0,0,1,1,1,1,1,1), /* b */
    BITMASK32(1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* c */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* d */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* e */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0)  /* f */
};
#define CHECK_MODRM2(v) CHECK_TABLE(modrm2_t, v)
#else
#define CHECK_MODRM2(v) (__extension__  ({                                     \
    register BYTE __a=(v)&0xfc, __b=(v)&0xfe;                                  \
    ((v)&0xf0)==0x90||((v)&0xf8)==0xb0||((v)&0xf6)==0xa4||                     \
    __a==0x00||__a==0xbc||__b==0xba||__b==0xc0||                               \
    (v)==0xa3||(v)==0xab||(v)==0xaf;                                           \
}))
#endif

/* CHECK_DATA12 */
#define CHECK_DATA12(v)     ((v)==0xa4||(v)==0xac||(v)==0xba)

/* CHECK_DATA662 */
#define CHECK_DATA662(v)    (((v)&0xf0)==0x80)

/* CHECK_MODRM */
#if defined(USE_T) && (USE_T & MODRM_T)
const static unsigned int modrm_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,  /* 0 */
              1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0), /* 1 */
    BITMASK32(1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,  /* 2 */
              1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,1,1,0,0,0,0, 0,1,0,1,0,0,0,0,  /* 6 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 7 */
    BITMASK32(1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,  /* 8 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 9 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* a */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* b */
    BITMASK32(1,1,0,0,1,1,1,1, 0,0,0,0,0,0,0,0,  /* c */
              1,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,1), /* d */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* e */
              0,0,0,0,0,0,1,1, 0,0,0,0,0,0,1,1)  /* f */
};
#define CHECK_MODRM(v) CHECK_TABLE(modrm_t, v)
#else
#define CHECK_MODRM(v) (__extension__  ({                                      \
    register BYTE __a=(v)&0xfc, __b=(v)&0xfe;                                  \
    ((v)&0xc4)==0x00||((v)&0xf0)==0x80||((v)&0xf8)==0xd8||((v)&0xf6)==0xf6||   \
    __a==0xc4||__a==0xd0||__b==0x62||__b==0xc0||                               \
    (v)==0x69||(v)==0x6b;                                                      \
}))
#endif

/* CHECK_TEST */
#define CHECK_TEST(v)   ((v)==0xf6||(v)==0xf7)

/* CHECK_DATA1 */
#if defined(USE_T) && (USE_T & DATA1_T)
const static unsigned int data1_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(0,0,0,0,1,0,0,0, 0,0,0,0,1,0,0,0,  /* 0 */
              0,0,0,0,1,0,0,0, 0,0,0,0,1,0,0,0), /* 1 */
    BITMASK32(0,0,0,0,1,0,0,0, 0,0,0,0,1,0,0,0,  /* 2 */
              0,0,0,0,1,0,0,0, 0,0,0,0,1,0,0,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,1,1,0,0,0,0,  /* 6 */
              1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1), /* 7 */
    BITMASK32(1,0,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 8 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 9 */
    BITMASK32(0,0,0,0,0,0,0,0, 1,0,0,0,0,0,0,0,  /* a */
              1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0), /* b */
    BITMASK32(1,1,0,0,0,0,1,0, 1,0,0,0,0,1,0,0,  /* c */
              0,0,0,0,1,1,0,0, 0,0,0,0,0,0,0,0), /* d */
    BITMASK32(1,1,1,1,1,1,1,1, 0,0,0,1,0,0,0,0,  /* e */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0)  /* f */
};
#define CHECK_DATA1(v) CHECK_TABLE(data1_t, v)
#else
#define CHECK_DATA1(v) (__extension__  ({                                      \
    register BYTE __a=(v)&0xf8, __b=(v)&0xfe;                                  \
    ((v)&0xf0)==0x70||((v)&0xc7)==0x04||                                       \
    __a==0xb0||__a==0xe0||__b==0x6a||__b==0x82||__b==0xc0||__b==0xd4||         \
    (v)==0x80||(v)==0xa8||(v)==0xc6||(v)==0xc8||(v)==0xcd||(v)==0xeb;          \
}))
#endif

/* CHECK_DATA2 */
#if defined(USE_T) && (USE_T & DATA2_T)
const static unsigned int data2_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 0 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 1 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 2 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 6 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 7 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 8 */
              0,0,0,0,0,0,0,0, 0,0,1,0,0,0,0,0), /* 9 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* a */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* b */
    BITMASK32(0,0,1,0,0,0,0,0, 1,0,1,0,0,0,0,0,  /* c */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* d */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,1,0,0,0,0,0,  /* e */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0)  /* f */
};
#define CHECK_DATA2(v) CHECK_TABLE(data2_t, v)
#else
#define CHECK_DATA2(v)                                                         \
    ((v)==0x9a||(v)==0xc2||(v)==0xc8||(v)==0xca||(v)==0xea)
#endif

/* CHECK_DATA66 */
#if defined(USE_T) && (USE_T & DATA66_T)
const static unsigned int data66_t[] = {
           /* 0 1 2 3 4 5 6 7  8 9 a b c d e f */
    BITMASK32(0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0,  /* 0 */
              0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0), /* 1 */
    BITMASK32(0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0,  /* 2 */
              0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0), /* 3 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 4 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 5 */
    BITMASK32(0,0,0,0,0,0,0,0, 1,1,0,0,0,0,0,0,  /* 6 */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* 7 */
    BITMASK32(0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  /* 8 */
              0,0,0,0,0,0,0,0, 0,0,1,0,0,0,0,0), /* 9 */
    BITMASK32(0,0,0,0,0,0,0,0, 0,1,0,0,0,0,0,0,  /* a */
              0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1), /* b */
    BITMASK32(0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,0,  /* c */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0), /* d */
    BITMASK32(0,0,0,0,0,0,0,0, 1,1,1,0,0,0,0,0,  /* e */
              0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0)  /* f */
};
#define CHECK_DATA66(v) CHECK_TABLE(data66_t, v)
#else
#define CHECK_DATA66(v)                                                        \
    (((v)&0xc7)==0x05||((v)&0xf8)==0xb8||((v)&0x7e)==0x68||                    \
    (v)==0x81||(v)==0x9a||(v)==0xa9||(v)==0xc7||(v)==0xea)
#endif

/* CHECK_MEM67 */
#define CHECK_MEM67(v)  (((v)&0xfc)==0xa0)

#endif
