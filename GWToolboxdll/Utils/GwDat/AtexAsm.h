#pragma once

#include <cstdint>

struct SImageData;
// C++ implementations (defined in AtexDecompress.cpp and AtexAsm.cpp)
void AtexSubCode1_Cpp(uint32_t* array1, uint32_t* array2, unsigned int count);
void AtexSubCode2_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode3_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode4_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode5_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize, unsigned int flag);
uint32_t AtexSubCode6_Cpp(uint32_t* output, uint32_t colorValue, uint32_t flag);
void AtexSubCode7_Cpp(uint32_t* outBuffer, unsigned int blockCount);
