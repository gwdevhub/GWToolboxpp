#pragma once

#include <cstdint>
#include <span>
#include <stop_token>

bool UnpackGWFile(std::span<const uint8_t> input, std::span<uint8_t> output, std::stop_token stop);

void UnpackGWDat(unsigned char* input, int insize, unsigned char*& output, int& outsize);
