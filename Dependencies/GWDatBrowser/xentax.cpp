
#include <memory.h>

unsigned char TableData1[112] = {
    0x00, 0x00, 0x00, 0xA0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x12, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x12, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x1F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x39, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x60, 0x01, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x4D, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x57, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xA0, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00
};
unsigned int *Table1 = ( unsigned int* ) TableData1;

unsigned char Table2[256] = {
    0x08, 0x09, 0x0A, 0x00, 0x07, 0x0B, 0x0C, 0x06, 0x29, 0x2A, 0xE0, 0x04, 0x05, 0x20, 0x28, 0x2B,
    0x2C, 0x40, 0x4A, 0x03, 0x0D, 0x25, 0x26, 0x27, 0x48, 0x49, 0x24, 0x47, 0x4B, 0x4C, 0x69, 0x6A,
    0x23, 0x46, 0x60, 0x63, 0x67, 0x68, 0x88, 0x89, 0xA0, 0xE8, 0x01, 0x02, 0x2D, 0x43, 0x44, 0x45,
    0x65, 0x66, 0x80, 0x87, 0x8A, 0xA8, 0xA9, 0xC0, 0xC9, 0xE9, 0x0E, 0x4D, 0x64, 0x6B, 0x6C, 0x84,
    0x85, 0x8B, 0xA4, 0xA5, 0xAA, 0xC8, 0xE5, 0x83, 0x86, 0xA6, 0xA7, 0xC7, 0xCA, 0xE7, 0x22, 0x2E,
    0x8C, 0xC4, 0xE4, 0xE6, 0x4E, 0x6D, 0xC6, 0xEC, 0x0F, 0x10, 0x11, 0x8D, 0xAB, 0xAC, 0xCC, 0xEA,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x21, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x41, 0x42, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
    0x5D, 0x5E, 0x5F, 0x61, 0x62, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x81, 0x82, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA1, 0xA2, 0xA3, 0xAD, 0xAE,
    0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
    0xBF, 0xC1, 0xC2, 0xC3, 0xC5, 0xCB, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
    0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xEB, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

unsigned char TableData3[768] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x99, 0x89, 0x80, 0x80, 0x40, 0x80, 0x40, 0x55, 0x80,
    0x42, 0x80, 0x80, 0x55, 0x80, 0x98, 0x80, 0x42, 0x80, 0x52, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x55, 0x88, 0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x5A, 0x80, 0x80, 0x52,
    0x80, 0x80, 0x80, 0x80, 0x92, 0x80, 0x80, 0x40, 0x80, 0x42, 0x80, 0x55, 0x80, 0x40, 0x5A, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x99, 0x99, 0x80, 0x80, 0x89, 0x88,
    0x40, 0x40, 0x99, 0x80, 0x8A, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x90, 0x90, 0x90, 0x90, 0x99,
    0x9A, 0x99, 0x84, 0x98, 0x80, 0x80, 0x9A, 0x99, 0x80, 0x80, 0x99, 0x80, 0x99, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x9A, 0x80, 0x80, 0x80, 0x8A, 0x99, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40,
    0x40, 0x80, 0x80, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x95, 0x80, 0x80,
    0x91, 0x90, 0x80, 0x80, 0x92, 0x99, 0x80, 0x80, 0x80, 0x40, 0x40, 0x40, 0x99, 0x8A, 0x80, 0x40,
    0x4A, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x06, 0x00,
    0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40, 0x00, 0x60, 0x00,
    0x80, 0x00, 0xC0, 0x00, 0x00, 0x01, 0x80, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x06,
    0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40, 0x00, 0x60,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x14, 0x18, 0x1C,
    0x20, 0x28, 0x30, 0x38, 0x40, 0x50, 0x60, 0x70, 0x80, 0xA0, 0xC0, 0xE0, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
    0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
    0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
    0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
    0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
    0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B,
    0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1C,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06,
    0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E,
    0x0F, 0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12,
    0x13, 0x13, 0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16,
    0x17, 0x17, 0x17, 0x17, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1A,
    0x1B, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1D, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char *Table3 = TableData3;
unsigned char *Table4 = TableData3 + 0x1DC;

unsigned char Table5[32] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06,
    0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0E
};

unsigned char TableData6[92] = {
    0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x06, 0x00, 0x08, 0x00, 0x0C, 0x00,
    0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40, 0x00, 0x60, 0x00, 0x80, 0x00, 0xC0, 0x00,
    0x00, 0x01, 0x80, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x06, 0x00, 0x08, 0x00, 0x0C,
    0x00, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40, 0x00, 0x60, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x28, 0x30, 0x38,
    0x40, 0x50, 0x60, 0x70, 0x80, 0xA0, 0xC0, 0xE0, 0xFF, 0x00, 0x00, 0x00
};
unsigned short *Table6 = ( unsigned short* ) TableData6;

struct HuffmanData {
    unsigned int HuffmanTable[0x200];
    unsigned int HelperArray[0x48];
    unsigned int *TempArray;
    unsigned int Var1,Var2;
};

class Decompress {
public:
    unsigned int ESIplus8, ESIplusC, ESIplus10;
    unsigned int *ptrInputData,*InputDataEnd;

    unsigned char* DecompressFile( unsigned int* Input, int InputSize, int &outsize ) {
        int _counter1;
        unsigned int _data, EBPminus8, _temp;
        int EBPminus18, EBPminus1c=0;

        HuffmanData HuffmanTree;
        memset( &HuffmanTree, 0, sizeof( HuffmanTree ) );
        HuffmanData HuffmanTree2;
        memset( &HuffmanTree2, 0, sizeof( HuffmanTree2 ) );

        // this part came from cmpdecompress

        ptrInputData = Input;
        InputDataEnd = Input + ( InputSize >> 2 );
        ESIplus8 = 0;
        ESIplus10 = 0;

        ESIplusC = ptrInputData[0] << 4;

        ptrInputData++;

        ESIplusC |= ptrInputData[0] >> 0x1c;
        ESIplus8 = 0x1c;
        ESIplus10 = ptrInputData[0] << 4;

        ptrInputData++;

        //cmpdecompress part ends here
        if(!outsize)
            outsize = Input[( InputSize >> 2 ) - 1];
        unsigned char *Output = new unsigned char[outsize];
        memset(Output,0,outsize);
        unsigned char *ptrOutput = Output;
        unsigned char *ptrOutputEnd = Output + outsize;

        EBPminus18 = ESIplusC >> 0x1c;
        ESIplusC = ( ESIplus10 >> 0x1c ) | ( ESIplusC << 4 );

        ESIplus10 = ESIplus10 << 4;
        ESIplus8 -= 4;

        _data = 0;

        if ( !outsize ) {
            return Output;
        }

        do {
            if ( !SetupNodesandTree( HuffmanTree ) )  {
                if ( HuffmanTree.TempArray )  {
                    delete[] HuffmanTree.TempArray;
                }
                delete[] Output;
                return 0;
            }
            if ( !SetupNodesandTree( HuffmanTree2 ) )  {
                if ( HuffmanTree2.TempArray ) {
                    delete[] HuffmanTree2.TempArray;
                }
                delete[] Output;
                return 0;
            }

            _counter1 = ESIplusC >> 0x1c;
            ESIplusC = ( ESIplus10 >> 0x1c ) | ( ESIplusC << 4 );

            if ( ESIplus8 < 4 ) {
                if ( ptrInputData == InputDataEnd ) {
                    ESIplus8 = ESIplus10 = 0;
                } else {
                    ESIplus10 = ptrInputData[0] << ( 4 - ESIplus8 );
                    ESIplus8 += 0x1c;
                    ESIplusC |= ptrInputData[0] >> ( ESIplus8 );
                    ptrInputData++;
                }
            } else {
                ESIplus10 *= 16;
                ESIplus8 -= 4;
            }

            _counter1 = ( ( unsigned int )( _counter1 + 1 ) << 0xc ) - 1;

            if ( _counter1 > -1 )
            do {
                if ( ptrOutput == ptrOutputEnd ) {
                    break;
                }

                int i = ( ESIplusC >> 0x18 ) * 2;
                _temp = HuffmanTree.HuffmanTable[i];
                _data = HuffmanTree.HuffmanTable[i+1];

                if ( _temp == -1 ) {
                    int i = 0;
                    while ( ESIplusC < HuffmanTree.HelperArray[i] ) {
                        i += 3;
                    }

                    _temp = HuffmanTree.HelperArray[i + 2];
                    _data = HuffmanTree.HelperArray[i + 1] - ( ( unsigned int )( ESIplusC - HuffmanTree.HelperArray[i] ) >> ( 0x20 - _temp ) );

                    if ( _data >= HuffmanTree.Var2 ) {
                        //printf( "Error\n" );
                        //throw error
                        return 0;
                    }

                    if ( !HuffmanTree.TempArray ) {
                        delete[] Output;
                        return 0;
                    }

                    _data = HuffmanTree.TempArray[_data];
                }
                if ( _temp >= 0x20 ) {
                    //printf( "Error\n" );
                    //throw error
                    return 0;
                }
                if ( _temp ) {
                    ESIplusC = ( ESIplus10 >> ( 0x20 - _temp ) ) | ( ESIplusC << _temp );
                }
                if ( _temp > ESIplus8 ) {
                    if ( ptrInputData == InputDataEnd ) {
                        ESIplus8 = ESIplus10 = 0;
                    } else {
                        ESIplus10 = ptrInputData[0];
                        ptrInputData ++;
                        EBPminus8 = ESIplus8 - _temp + 0x20;

                        ESIplusC |= ( ESIplus10 >> EBPminus8 );
                        ESIplus10 = ESIplus10 << ( _temp - ESIplus8 );
                        ESIplus8 = EBPminus8;
                    }
                } else {
                    ESIplus8 -= _temp;
                    ESIplus10 = ESIplus10 << _temp;
                }

                if ( _data < 0x100 ) {
                    ptrOutput++[0] = _data;
                } else {
                    _temp = Table4[_data];
                    _data = Table3[_data];

                    if ( _temp ) {
                        _data |= ESIplusC >> ( 0x20 - _temp );
                        if ( _temp >= 0x20 ) {
                            //printf( "Error\n" );
                            //throw error
                            return 0;
                        }
                        ESIplusC = ( ESIplus10 >> ( 0x20 - _temp ) ) | ( ESIplusC << _temp );

                        if ( _temp > ESIplus8 ) {
                            if ( ptrInputData == InputDataEnd ) {
                                ESIplus8 = ESIplus10 = 0;
                            } else {
                                ESIplus10 = ptrInputData[0];
                                ptrInputData ++;
                                EBPminus8 =  ESIplus8 - _temp + 0x20;
                                ESIplusC |= ESIplus10 >> EBPminus8;
                                ESIplus10 = ESIplus10 << ( _temp - ESIplus8 );
                                ESIplus8 = EBPminus8;
                            }
                        } else {
                            ESIplus10 = ESIplus10 << _temp;
                            ESIplus8 -= _temp;
                        }
                    }

                    EBPminus1c = EBPminus18 + _data + 1;
                    int i = ( ESIplusC >> 0x18 ) * 2;
                    _temp = HuffmanTree2.HuffmanTable[i];
                    _data = HuffmanTree2.HuffmanTable[i + 1];

                    if ( _temp == -1 ) {
                        int i = 0;
                        while ( ESIplusC < HuffmanTree2.HelperArray[i] ) {
                            i += 3;
                        }

                        _temp = HuffmanTree2.HelperArray[i + 2];
                        _data = HuffmanTree2.HelperArray[i + 1] - ( ( unsigned int ) ( ESIplusC - HuffmanTree2.HelperArray[i] ) >> ( 0x20 - _temp ) );

                        if ( _data >= HuffmanTree2.Var2 ) {
                            //printf( "Error\n" );
                            //throw error
                            return 0;
                        }

                        _data = HuffmanTree2.TempArray[_data];
                    }

                    if ( _temp >= 0x20 ) {
                        //printf("Error\n");
                        //throw error
                        return 0;
                    }

                    if ( _temp ) {
                        ESIplusC = ( ESIplus10 >> ( 0x20 - _temp ) ) | ( ESIplusC << _temp );
                    }

                    if ( _temp > ESIplus8 ) {
                        if ( ptrInputData == InputDataEnd ) {
                            ESIplus8 = ESIplus10 = 0;
                        } else {
                            ESIplus10 = ptrInputData[0];
                            ptrInputData ++;
                            EBPminus8 = ESIplus8 - _temp + 0x20;
                            ESIplusC |= ESIplus10 >> EBPminus8;

                            ESIplus10 = ESIplus10 << ( _temp - ESIplus8 );
                            ESIplus8 = EBPminus8;
                        }
                    } else {
                        ESIplus8 -= _temp;
                        ESIplus10 = ESIplus10 << _temp;
                    }

                    _temp = Table5[_data];

                    int backtrack = Table6[_data];

                    if ( _temp ) {
                        _data = backtrack | ESIplusC >> ( 0x20 - _temp );
                        if ( _temp >= 0x20 ) {
                            //printf("Error\n");
                            //throw error
                            return 0;
                        }

                        ESIplusC = ( ESIplus10 >> ( 0x20 - _temp ) ) | ( ESIplusC << _temp );

                        if ( _temp>ESIplus8 ) {
                            if ( ptrInputData == InputDataEnd ) {
                                ESIplus8 = ESIplus10 = 0;
                            } else {
                                ESIplus10 = ptrInputData[0];
                                ptrInputData ++;
                                EBPminus8 = ESIplus8 - _temp + 0x20;
                                ESIplusC |= ESIplus10 >> EBPminus8;
                                ESIplus10 = ESIplus10 << ( _temp - ESIplus8 );
                                ESIplus8 = EBPminus8;
                            }
                        } else {
                            ESIplus10 = ESIplus10 << _temp;
                            ESIplus8 -= _temp;
                        }
                        backtrack = _data;
                    }

                    if ( ptrOutput + EBPminus1c > ptrOutputEnd || backtrack >= ( int )( ptrOutput - Output ) ) {
                        //this shouldn't ever be called
                        if ( HuffmanTree2.TempArray ) {
                            delete[] HuffmanTree2.TempArray;
                        }
                        if ( HuffmanTree.TempArray ) {
                            delete[] HuffmanTree.TempArray;
                        }
                        return Output;
                    } else {
                        for ( int x = 0; x < EBPminus1c; x++) {
                            ptrOutput++[0] = ptrOutput[-backtrack - 1];
                        }
                    }
                }

                _counter1--;
                _data = 0;

            } while ( _counter1 >= 0);

            if ( HuffmanTree2.TempArray ) {
                delete[] HuffmanTree2.TempArray;
            }

            HuffmanTree2.TempArray = 0;
            HuffmanTree2.Var1 = 0;
            HuffmanTree2.Var2 = 0;

            if ( HuffmanTree.TempArray ) {
                delete[] HuffmanTree.TempArray;
            }

            HuffmanTree.TempArray = 0;
            HuffmanTree.Var1 = 0;
            HuffmanTree.Var2 = 0;

        } while ( ptrOutputEnd != ptrOutput );

        return Output;
    }

    bool SetupNodesandTree( HuffmanData &HData ) {
        unsigned int EBPminus128[0x40];
        unsigned int *EBPminus20, *EBPminus18, EBPminus10, EBPminus8, EBPminus14;
        int EBPminus4, EBPminus1C, EBPminus24, EBPminus28, arg_0;

        HData.TempArray = 0;
        HData.Var1 = 0;
        HData.Var2 = 0;

        EBPminus14 = ESIplusC >> 0x10;
        ESIplusC = ( ESIplus10 >> 0x10 ) | ( ESIplusC << 0x10 );

        if ( ESIplus8 < 0x10 ) {
            if (ptrInputData == InputDataEnd ) {
                ESIplus8 = ESIplus10 = 0;
            } else {
                ESIplus10 = ptrInputData[0];
                ptrInputData++;
                ESIplusC |= ESIplus10 >> ( ESIplus8 + 0x10 );
                ESIplus10 = ESIplus10 << ( 0x10 - ESIplus8 );
                ESIplus8 += 0x10;
                arg_0 = ESIplus8;
            }
        } else {
            ESIplus8 -= 0x10;
            ESIplus10 = ESIplus10 << 0x10;
        }

        EBPminus20 = new unsigned int[EBPminus14];
        memset( EBPminus20, 0, EBPminus14 * 4 );
        EBPminus1C = 0;

        memset( EBPminus128, 0, 0x80);
        memset( EBPminus128 + 0x20, 0xff, 0x80);
        //Helper2( EBPminus128, 0x80 );
        //Helper3( EBPminus128 + 0x80, EDX | 0xff, 0x80);

        EBPminus8 = EBPminus10 = EBPminus14-1;

        if ( EBPminus14 - 1 != -1 ) {
            do  {
                int i = 0;
                while ( ESIplusC < Table1[i] ) {
                    i += 2;
                }

                int j = ( i * 4 + 0x18 ) >> 3;
                EBPminus4 = ( 0x20 - j );

                arg_0 = Table2[Table1[i + 1] - ( ( ESIplusC - Table1[i] ) >> EBPminus4 )];
                if ( j >= 0x20 ) {
                    //printf( "Error\n" );
                    //throw error
                    delete[] EBPminus20;
                    return false;
                }
                if ( j ) {
                    ESIplusC = ( ESIplus10 >> EBPminus4 ) | ( ESIplusC << j );
                }
                if ( ( unsigned ) j > ESIplus8 ) {
                    if (ptrInputData == InputDataEnd) {
                        ESIplus8 = ESIplus10 = 0;
                    } else {
                        ESIplus10 = ptrInputData[0];
                        ptrInputData++;
                        EBPminus4 = ESIplus8 - j + 0x20;
                        ESIplusC |= ESIplus10 >> EBPminus4;
                        ESIplus10 = ESIplus10 << ( j - ESIplus8 );
                        ESIplus8 = EBPminus4;
                    }
                } else {
                    ESIplus10 = ESIplus10 << j;
                    ESIplus8 -= j;
                }

                j = arg_0 >> 5;
                arg_0 &= 0x1f;
                if ( arg_0 > 0x1f ) {
                    //printf( "Error\n" );
                    //throw error
                    delete[] EBPminus20;
                    return false;
                }

                if ( ( unsigned ) j > EBPminus8 ) {
                    //this returns
                    delete[] EBPminus20;
                    return true;
                }

                if ( arg_0 || EBPminus14 < 2 ) {
                    EBPminus128[arg_0] += j+1;
                    unsigned int *ptr1 = EBPminus128 + 0x20 + arg_0;
                    EBPminus1C += j+1;
                    unsigned int *ptr2 = EBPminus20 + EBPminus8;

                    do {
                        if ( EBPminus8 >= EBPminus14 ) {
                            //printf( "Error\n" );
                            //throw error
                            delete[] EBPminus20;
                            return false;
                        }

                        ptr2[0] = ptr1[0];
                        ptr1[0] = EBPminus8;
                        ptr2 --;
                        EBPminus8 --;
                        j --;

                    } while ( j + 1 && EBPminus8 != -1);
                } else {
                    EBPminus8 -= j + 1;
                }

            } while ( EBPminus8 != -1 );

        }

        // processhuffmantree

        if ( EBPminus14 && !EBPminus1C ) {
            EBPminus20[EBPminus10] = EBPminus128[0x20];
            EBPminus128[0x20] = EBPminus10;
            EBPminus128[0] = 1;
            EBPminus1C = 1;
        }

        memset( HData.HuffmanTable, 0, 0x800 );

        arg_0 = 0;
        EBPminus24 = 0;
        EBPminus8 = 0;
        EBPminus4 = 8;
        EBPminus18 = EBPminus128 + 0x20;

        do {
            int v = EBPminus18[0];
            if ( EBPminus18[0] != -1 ) {
                EBPminus10 = 1 << arg_0;
                do {
                    if ( ( unsigned ) EBPminus8 >= EBPminus10 || ( unsigned ) v >= EBPminus14 ) {
                        //return here too
                        delete[] EBPminus20;
                        return true;
                    }

                    int i = ( 1 << EBPminus4 ) - 1;
                    if ( 1 << EBPminus4 ) {
                        EBPminus28 = EBPminus8 << ( 8 - arg_0 );
                        do {
                            if ( ( unsigned ) ( EBPminus28 | i ) >= 0x100 ) {
                                //printf( "Error\n" );
                                //throw error
                                delete[] EBPminus20;
                                return false;
                            }
                            HData.HuffmanTable[( EBPminus28 | i ) * 2] = arg_0;
                            HData.HuffmanTable[( EBPminus28 | i ) * 2 + 1] = v;
                            i --;
                        } while ( i + 1 );
                    }

                    v = EBPminus20[v];
                    EBPminus24++;
                    EBPminus8--;
                } while (v != -1);
            }

            EBPminus18++;
            EBPminus4--;
            EBPminus8 = 2 * EBPminus8 + 1;
            arg_0 ++;
        } while ( arg_0 <= 8 );

        if ( EBPminus24 > EBPminus1C ) {
            //printf( "Error\n" );
            // throw error
            delete[] EBPminus20;
            return false;
        }

        if ( EBPminus24 != EBPminus1C ) {
            EBPminus10 = EBPminus1C - EBPminus24;
            if ( HData.Var2 > EBPminus10 ) {
                HData.Var2 = EBPminus10;
            }

            if ( HData.Var1 != EBPminus10 ) {
                if ( HData.TempArray ) {
                    delete[] HData.TempArray;
                }
                HData.TempArray = new unsigned int[EBPminus10];
                memset( HData.TempArray, 0, EBPminus10 * 4 );
                HData.Var1 = EBPminus10;
            }
            if ( HData.Var2 < EBPminus10 ) {
                HData.Var2=EBPminus10;
            }

            unsigned int v = 0;
            if ( arg_0 <= 0x1f ) {
                unsigned int *ptr = EBPminus128 + 0x20 + arg_0;
                EBPminus18 = HData.HelperArray + 1;
                do {
                    if ( ptr[0] != -1 ) {
                        unsigned int e = ptr[0];
                        if ( e != -1 ) {
                            EBPminus10 = 1 << arg_0;
                            do {
                                if ( EBPminus8 > EBPminus10 || ( unsigned )e > EBPminus14 ) {
                                    //Wrapperx
                                    //return here too
                                    delete[] EBPminus20;
                                    return true;
                                }
                                HData.HuffmanTable[( EBPminus8 >> ( arg_0 - 8 ) ) * 2 ] = -1;
                                if ( v >= HData.Var2 ) {
                                    //printf( "Error\n" );
                                    //throw error
                                    delete[] EBPminus20;
                                    return false;
                                }
                                HData.TempArray[v] = e;
                                e = EBPminus20[e];
                                v++;
                                EBPminus8--;
                            } while ( e != -1);
                        }

                        EBPminus18 += 3;
                        EBPminus18[-4] = ( unsigned int )( EBPminus8 + 1 ) << ( 0x20 - arg_0 );
                        EBPminus18[-3] = v - 1;
                        EBPminus18[-2] = arg_0;
                    }

                    ptr++;
                    arg_0++;
                    EBPminus8 += EBPminus8 + 1;
                } while ( arg_0 <= 0x1f );
                delete[] EBPminus20;
                return true;
            }
        }
        delete[] EBPminus20;
        return true;
    }

};

void UnpackGWDat( unsigned char *input, int insize, unsigned char *&output, int &outsize ) {
    Decompress d;
    output = d.DecompressFile( ( unsigned int* )input, insize, outsize );
}
