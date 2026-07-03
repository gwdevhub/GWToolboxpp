#include <cstdint>
#include <cstring>

#include "AtexAsm.h"
#include "AtexDecompress.h"


void AtexSubCode1_Cpp(uint32_t* array1, uint32_t* array2, unsigned int count)
{
	for (auto i = 0u; i < count; ++i) {
		const uint32_t mask = 1 << (i & 0x1F);
		if ((mask & 0xC0000003) != 0 || ((1 << ((i >> 6) & 0x1F)) & 0xC0000003) != 0) {
			const uint32_t array_index = i >> 5;
			array1[array_index] |= mask;
			array2[array_index] |= mask;
		}
	}
}

void AtexSubCode2_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, uint32_t blockCount, uint32_t blockSize)
{
	if (blockCount == 0) {
		return;
	}

	uint32_t block_index = 0;
	int extractedBits;
	uint32_t* dataPosition;
	uint32_t currentWord;

	while (block_index < blockCount)
	{
		const auto shiftLeft = imageData->currentBits >> 26;
		const auto shiftValue = byte_79053C[2 * shiftLeft];
		int remainingBits = byte_79053D[2 * shiftLeft] + 1;

		if (shiftValue) {
			imageData->currentBits = (imageData->currentBits << shiftValue) | (imageData->nextBits >> (32 - shiftValue));
		}

		uint32_t bitCount = imageData->remainingBits;

		if (shiftValue > bitCount)
		{
			dataPosition = imageData->DataPos;

			if (dataPosition == imageData->EndPos)
			{
				extractedBits = 0;
				imageData->nextBits = 0;
			}
			else
			{
				currentWord = *dataPosition;
				imageData->DataPos = dataPosition + 1;
				imageData->nextBits = currentWord;
				imageData->currentBits |= currentWord >> (bitCount - shiftValue + 32);
				imageData->nextBits = currentWord << (shiftValue - bitCount);
				extractedBits = bitCount - shiftValue + 32;
			}
		}
		else
		{
			extractedBits = bitCount - shiftValue;
			imageData->nextBits <<= shiftValue;
		}

		const uint64_t combinedValue = (static_cast<uint64_t>(imageData->currentBits) << 32) | imageData->nextBits;
		imageData->remainingBits = extractedBits;
		imageData->currentBits = static_cast<uint32_t>(combinedValue >> 31);
		int bitMask = static_cast<int>(combinedValue >> 63);

		if (imageData->remainingBits)
		{
			imageData->nextBits = static_cast<uint32_t>(combinedValue << 1);
			imageData->remainingBits -= 1;
		}
		else
		{
			dataPosition = imageData->DataPos;

			if (dataPosition == imageData->EndPos)
			{
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
			else
			{
				const auto curPos = *dataPosition;
				imageData->DataPos = dataPosition + 1;
				currentWord = curPos;
				imageData->currentBits |= currentWord >> 31;
				imageData->remainingBits = 31;
				imageData->nextBits = 2 * currentWord;
			}
		}

		while (remainingBits > 0 && block_index < blockCount)
		{
			const int bitPosition = 1 << (block_index & 0x1F);
			const int bufferIndex = block_index >> 5;

			if ((bitPosition & dcmpBuffer2[bufferIndex]) == 0)
			{
				if (bitMask)
				{
					outBuffer[0] = static_cast<uint32_t>(-2);
					outBuffer[1] = static_cast<uint32_t>(-1);
					dcmpBuffer2[bufferIndex] |= bitPosition;
					dcmpBuffer1[bufferIndex] |= bitPosition;
					bitMask = static_cast<int>(combinedValue >> 63);
				}
				remainingBits -= 1;
			}

			block_index++;
			outBuffer += blockSize;
		}

		while (block_index < blockCount && (1 << (block_index & 0x1F) & dcmpBuffer2[block_index >> 5]) != 0)
		{
			block_index++;
			outBuffer += blockSize;
		}
	}
}

void AtexSubCode3_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
	SImageData* imageData, unsigned int blockCount, unsigned int blockSize)
{
	if (blockCount == 0) {
		return;
	}

	uint32_t nextBits = imageData->nextBits;
	uint32_t currentBits = imageData->currentBits;
	uint32_t remainingBits = imageData->remainingBits;

	uint32_t extractedBits = (currentBits >> 28) & 0xF;
	currentBits = (currentBits << 4) | (nextBits >> 28);

	if (remainingBits < 4) {
		if (imageData->DataPos != imageData->EndPos) {
			uint32_t newData = *imageData->DataPos++;
			currentBits |= newData >> (remainingBits + 28);
			nextBits = newData << (4 - remainingBits);
			remainingBits += 28;
		}
		else {
			nextBits = 0;
			remainingBits = 0;
		}
	}
	else {
		nextBits <<= 4;
		remainingBits -= 4;
	}

	imageData->currentBits = currentBits;
	imageData->nextBits = nextBits;
	imageData->remainingBits = remainingBits;

	uint32_t colorPattern = extractedBits;
	colorPattern = (colorPattern << 4) | extractedBits;
	colorPattern = (colorPattern << 8) | colorPattern;
	colorPattern = (colorPattern << 16) | colorPattern;

	uint32_t colorTable[8];
	colorTable[0] = 0;
	colorTable[1] = 0;
	colorTable[2] = 0;
	colorTable[3] = 0;
	colorTable[4] = colorPattern;
	colorTable[5] = colorPattern;
	colorTable[6] = 0;
	colorTable[7] = 0;

	unsigned int blockIndex = 0;

	while (blockIndex < blockCount) {
		currentBits = imageData->currentBits;
		unsigned int shiftIndex = currentBits >> 26;
		unsigned int bitsToRead = byte_79053D[shiftIndex * 2] + 1;
		unsigned int shiftAmount = byte_79053C[shiftIndex * 2];

		if (shiftAmount != 0) {
			currentBits = (currentBits << shiftAmount) |
				(imageData->nextBits >> (32 - shiftAmount));
		}

		remainingBits = imageData->remainingBits;
		if (shiftAmount > remainingBits) {
			if (imageData->DataPos != imageData->EndPos) {
				uint32_t newData = *imageData->DataPos++;
				currentBits |= newData >> (remainingBits + 32 - shiftAmount);
				imageData->nextBits = newData << (shiftAmount - remainingBits);
				imageData->remainingBits = remainingBits + 32 - shiftAmount;
			}
			else {
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
		}
		else {
			imageData->remainingBits = remainingBits - shiftAmount;
			imageData->nextBits <<= shiftAmount;
		}

		imageData->currentBits = currentBits;

		currentBits = imageData->currentBits;
		uint32_t flagBit1 = currentBits >> 31;
		imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);

		if (imageData->remainingBits > 0) {
			imageData->nextBits <<= 1;
			imageData->remainingBits--;
		}
		else {
			if (imageData->DataPos != imageData->EndPos) {
				uint32_t newData = *imageData->DataPos++;
				imageData->currentBits |= newData >> 31;
				imageData->nextBits = newData << 1;
				imageData->remainingBits = 31;
			}
			else {
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
		}

		uint32_t colorIndex = flagBit1;

		if (flagBit1 != 0) {
			currentBits = imageData->currentBits;
			uint32_t flagBit2 = currentBits >> 31;
			imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);

			if (imageData->remainingBits > 0) {
				imageData->nextBits <<= 1;
				imageData->remainingBits--;
			}
			else {
				if (imageData->DataPos != imageData->EndPos) {
					uint32_t newData = *imageData->DataPos++;
					imageData->currentBits |= newData >> 31;
					imageData->nextBits = newData << 1;
					imageData->remainingBits = 31;
				}
				else {
					imageData->nextBits = 0;
					imageData->remainingBits = 0;
				}
			}

			colorIndex = flagBit1 + flagBit2;
		}

		while (bitsToRead > 0 && blockIndex < blockCount) {
			uint32_t bitMask = 1u << (blockIndex & 0x1F);
			uint32_t arrayIndex = blockIndex >> 5;

			if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
				if (colorIndex != 0) {
					outBuffer[0] = colorTable[colorIndex * 2];
					outBuffer[1] = colorTable[colorIndex * 2 + 1];
					dcmpBuffer1[arrayIndex] |= bitMask;
				}
				bitsToRead--;
			}

			blockIndex++;
			outBuffer += blockSize;
		}

		while (blockIndex < blockCount) {
			uint32_t bitMask = 1u << (blockIndex & 0x1F);
			uint32_t arrayIndex = blockIndex >> 5;

			if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
				break;
			}

			blockIndex++;
			outBuffer += blockSize;
		}
	}
}

void AtexSubCode4_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
	SImageData* imageData, unsigned int blockCount, unsigned int blockSize)
{
	if (blockCount == 0) {
		return;
	}

	uint32_t nextBits = imageData->nextBits;
	uint32_t currentBits = imageData->currentBits;
	uint32_t remainingBits = imageData->remainingBits;

	uint32_t extractedBits = (currentBits >> 24) & 0xFF;
	currentBits = (currentBits << 8) | (nextBits >> 24);

	if (remainingBits < 8) {
		if (imageData->DataPos != imageData->EndPos) {
			uint32_t newData = *imageData->DataPos++;
			currentBits |= newData >> (remainingBits + 24);
			nextBits = newData << (8 - remainingBits);
			remainingBits += 24;
		}
		else {
			nextBits = 0;
			remainingBits = 0;
		}
	}
	else {
		nextBits <<= 8;
		remainingBits -= 8;
	}

	imageData->currentBits = currentBits;
	imageData->nextBits = nextBits;
	imageData->remainingBits = remainingBits;

	uint32_t colorPattern = (extractedBits << 8) | extractedBits;

	uint32_t colorTable[8];
	colorTable[0] = 0;
	colorTable[1] = 0;
	colorTable[2] = 0;
	colorTable[3] = 0;
	colorTable[4] = colorPattern;
	colorTable[5] = 0;
	colorTable[6] = 0;
	colorTable[7] = 0;

	unsigned int blockIndex = 0;

	while (blockIndex < blockCount) {
		currentBits = imageData->currentBits;
		unsigned int shiftIndex = currentBits >> 26;
		unsigned int bitsToRead = byte_79053D[shiftIndex * 2] + 1;
		unsigned int shiftAmount = byte_79053C[shiftIndex * 2];

		if (shiftAmount != 0) {
			currentBits = (currentBits << shiftAmount) |
				(imageData->nextBits >> (32 - shiftAmount));
		}

		remainingBits = imageData->remainingBits;
		if (shiftAmount > remainingBits) {
			if (imageData->DataPos != imageData->EndPos) {
				uint32_t newData = *imageData->DataPos++;
				currentBits |= newData >> (remainingBits + 32 - shiftAmount);
				imageData->nextBits = newData << (shiftAmount - remainingBits);
				imageData->remainingBits = remainingBits + 32 - shiftAmount;
			}
			else {
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
		}
		else {
			imageData->remainingBits = remainingBits - shiftAmount;
			imageData->nextBits <<= shiftAmount;
		}

		imageData->currentBits = currentBits;

		currentBits = imageData->currentBits;
		uint32_t flagBit1 = currentBits >> 31;
		imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);

		if (imageData->remainingBits > 0) {
			imageData->nextBits <<= 1;
			imageData->remainingBits--;
		}
		else {
			if (imageData->DataPos != imageData->EndPos) {
				uint32_t newData = *imageData->DataPos++;
				imageData->currentBits |= newData >> 31;
				imageData->nextBits = newData << 1;
				imageData->remainingBits = 31;
			}
			else {
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
		}

		uint32_t colorIndex = flagBit1;

		if (flagBit1 != 0) {
			currentBits = imageData->currentBits;
			uint32_t flagBit2 = currentBits >> 31;
			imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);

			if (imageData->remainingBits > 0) {
				imageData->nextBits <<= 1;
				imageData->remainingBits--;
			}
			else {
				if (imageData->DataPos != imageData->EndPos) {
					uint32_t newData = *imageData->DataPos++;
					imageData->currentBits |= newData >> 31;
					imageData->nextBits = newData << 1;
					imageData->remainingBits = 31;
				}
				else {
					imageData->nextBits = 0;
					imageData->remainingBits = 0;
				}
			}

			colorIndex = flagBit1 + flagBit2;
		}

		while (bitsToRead > 0 && blockIndex < blockCount) {
			uint32_t bitMask = 1u << (blockIndex & 0x1F);
			uint32_t arrayIndex = blockIndex >> 5;

			if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
				if (colorIndex != 0) {
					outBuffer[0] = colorTable[colorIndex * 2];
					outBuffer[1] = colorTable[colorIndex * 2 + 1];
					dcmpBuffer1[arrayIndex] |= bitMask;
				}
				bitsToRead--;
			}

			blockIndex++;
			outBuffer += blockSize;
		}

		while (blockIndex < blockCount) {
			uint32_t bitMask = 1u << (blockIndex & 0x1F);
			uint32_t arrayIndex = blockIndex >> 5;

			if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
				break;
			}

			blockIndex++;
			outBuffer += blockSize;
		}
	}
}

uint32_t AtexSubCode6_Cpp(uint32_t* output, uint32_t colorValue, uint32_t flag)
{
	uint32_t r = colorValue & 0xFF;
	uint32_t g = (colorValue >> 8) & 0xFF;
	uint32_t b = (colorValue >> 16) & 0xFF;

	uint32_t r_adj = r - (r >> 5);
	uint32_t g_adj = g - (g >> 6);
	uint32_t b_adj = b - (b >> 5);

	r_adj >>= 3;
	g_adj >>= 2;
	b_adj >>= 3;

	uint32_t r_base = r_adj;
	uint32_t g_base = g_adj;
	uint32_t b_base = b_adj;

	uint32_t qr = (r_adj >> 2) + r_adj * 8;
	uint32_t qg = (g_adj >> 4) + g_adj * 4;
	uint32_t qb = (b_adj >> 2) + b_adj * 8;

	r_adj++;
	g_adj++;
	b_adj++;

	uint32_t qr_next = (r_adj >> 2) + r_adj * 8;
	uint32_t qg_next = (g_adj >> 4) + g_adj * 4;
	uint32_t qb_next = (b_adj >> 2) + b_adj * 8;

	uint32_t dr = qr_next - qr;
	uint32_t dg = qg_next - qg;
	uint32_t db = qb_next - qb;

	uint32_t cr = 0, cg = 0, cb = 0;
	if (dr != 0) {
		cr = (r * 12 - qr * 12) / dr;
	}
	if (dg != 0) {
		cg = (g * 12 - qg * 12) / dg;
	}
	if (db != 0) {
		cb = (b * 12 - qb * 12) / db;
	}

	struct {
		uint32_t val1;
		uint32_t val2;
	} palette[3];

	uint32_t values[3] = { cr, cg, cb };
	uint32_t bases[3] = { r_base, g_base, b_base };

	for (int i = 0; i < 3; i++) {
		uint32_t val = values[i];
		uint32_t base = bases[i];

		if (val < 2) {
			palette[i].val1 = base;
			palette[i].val2 = base;
		}
		else if (val < 6) {
			palette[i].val1 = base;
			palette[i].val2 = base + 1;
		}
		else if (val < 10) {
			palette[i].val1 = base + 1;
			palette[i].val2 = base;
		}
		else {
			palette[i].val1 = base + 1;
			palette[i].val2 = base + 1;
		}
	}

	uint32_t color1_val1 = palette[0].val1 | (palette[1].val1 << 5) | (palette[2].val1 << 11);
	uint32_t color1_val2 = palette[0].val2 | (palette[1].val2 << 5) | (palette[2].val2 << 11);

	uint32_t color1, color2;

	uint32_t score = 0;
	uint32_t count = 0;

	for (int i = 0; i < 3; i++) {
		if (palette[i].val1 != palette[i].val2) {
			if (palette[i].val1 == bases[i]) {
				score += values[i];
			}
			else {
				score += (12 - values[i]);
			}
			count++;
		}
	}

	uint32_t avg = 0;
	if (count > 0) {
		avg = (score + count / 2) / count;
	}

	uint32_t swap = 0;
	if (flag != 0) {
		if ((avg == 5 || avg == 6) || count == 0) {
			swap = 1;
		}
	}

	color1 = color1_val1;
	color2 = color1_val2;

	if (count == 0 && swap == 0) {
		if (color2 != 0xFFFF) {
			avg = 0;
			color2++;
		}
		else {
			avg = 12;
			color1--;
		}
	}

	if ((color1 < color2) != swap) {
		uint32_t temp = color1;
		color1 = color2;
		color2 = temp;
		avg = 12 - avg;
	}

	uint32_t table;
	if (swap != 0) {
		table = 2;
	}
	else if (avg < 2) {
		table = 0;
	}
	else if (avg < 6) {
		table = 2;
	}
	else if (avg < 10) {
		table = 3;
	}
	else {
		table = 1;
	}

	output[0] = (color2 << 16) | color1;

	uint32_t pattern = table * 5;
	pattern = (pattern << 4) | pattern;
	pattern = (pattern << 8) | pattern;
	pattern = (pattern << 16) | pattern;
	output[1] = pattern;

	return avg | ((table * 5) << 16);
}

void AtexSubCode5_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
	SImageData* imageData, unsigned int blockCount, unsigned int blockSize,
	unsigned int flag)
{
	if (blockCount == 0) {
		return;
	}

	uint32_t currentBits = imageData->currentBits;
	uint32_t nextBits = imageData->nextBits;
	uint32_t remainingBits = imageData->remainingBits;

	uint32_t colorValue = (currentBits >> 8) | 0xFF000000;

	uint32_t newCurrentBits = (currentBits << 24) | (nextBits >> 8);

	if (remainingBits >= 24) {
		nextBits <<= 24;
		remainingBits -= 24;
	}
	else {
		if (imageData->DataPos != imageData->EndPos) {
			uint32_t newData = *imageData->DataPos++;
			uint32_t bitsToShift = remainingBits + 8;
			newCurrentBits |= newData >> bitsToShift;
			nextBits = newData << (24 - remainingBits);
			remainingBits = bitsToShift;
		}
		else {
			nextBits = 0;
			remainingBits = 0;
		}
	}

	imageData->currentBits = newCurrentBits;
	imageData->nextBits = nextBits;
	imageData->remainingBits = remainingBits;

	uint32_t colorData[2];
	uint32_t ediValue = AtexSubCode6_Cpp(colorData, colorValue, flag);

	for (unsigned int blockIdx = 0; blockIdx < blockCount; ) {
		uint32_t huffIdx = (imageData->currentBits >> 26) & 0x3F;
		uint32_t huffBits = byte_79053C[huffIdx * 2];
		uint32_t huffValue = byte_79053D[huffIdx * 2] + 1;

		if (huffBits < 32) {
			if (huffBits != 0) {
				imageData->currentBits = (imageData->currentBits << huffBits) |
					(imageData->nextBits >> (32 - huffBits));
			}

			if (huffBits <= imageData->remainingBits) {
				imageData->nextBits <<= huffBits;
				imageData->remainingBits -= huffBits;
			}
			else {
				if (imageData->DataPos != imageData->EndPos) {
					uint32_t newData = *imageData->DataPos++;
					uint32_t bitsNeeded = huffBits - imageData->remainingBits;
					imageData->currentBits |= newData >> (32 - bitsNeeded);
					imageData->nextBits = newData << bitsNeeded;
					imageData->remainingBits = 32 - bitsNeeded;
				}
				else {
					imageData->nextBits = 0;
					imageData->remainingBits = 0;
				}
			}
		}

		uint32_t bit = (imageData->currentBits >> 31) & 1;
		imageData->currentBits = (imageData->currentBits << 1) | (imageData->nextBits >> 31);

		if (imageData->remainingBits >= 1) {
			imageData->nextBits <<= 1;
			imageData->remainingBits--;
		}
		else {
			if (imageData->DataPos != imageData->EndPos) {
				uint32_t newData = *imageData->DataPos++;
				imageData->currentBits |= newData >> 31;
				imageData->nextBits = newData << 1;
				imageData->remainingBits = 31;
			}
			else {
				imageData->nextBits = 0;
				imageData->remainingBits = 0;
			}
		}

		uint32_t remaining = huffValue;
		while (remaining > 0 && blockIdx < blockCount) {
			uint32_t blockOffset = blockIdx & 0x1F;
			uint32_t blockWord = blockIdx >> 5;
			uint32_t blockMask = 1 << blockOffset;

			if (!(dcmpBuffer2[blockWord] & blockMask)) {
				if (bit) {
					outBuffer[0] = colorData[0];
					outBuffer[1] = colorData[1];
					dcmpBuffer2[blockWord] |= blockMask;
				}
				remaining--;
			}

			outBuffer += blockSize;
			blockIdx++;
		}

		while (blockIdx < blockCount) {
			uint32_t blockOffset = blockIdx & 0x1F;
			uint32_t blockWord = blockIdx >> 5;
			uint32_t blockMask = 1 << blockOffset;

			if (!(dcmpBuffer2[blockWord] & blockMask)) {
				break;
			}

			outBuffer += blockSize;
			blockIdx++;
		}
	}
}

void AtexSubCode7_Cpp(uint32_t* outBuffer, unsigned int blockCount)
{
	if (blockCount == 0) {
		return;
	}

	uint32_t* baseBuffer = outBuffer;

	for (unsigned int blockIdx = 0; blockIdx < blockCount; blockIdx++) {
		unsigned int posLow = blockIdx & 0x3F;
		unsigned int posHigh = blockIdx >> 6;

		unsigned int maskLow = 1 << (posLow & 0x1F);
		unsigned int maskHigh = 1 << (posHigh & 0x1F);

		bool needsLowSwizzle = (maskLow & 0xC0000003) != 0;
		bool needsHighSwizzle = (maskHigh & 0xC0000003) != 0;

		if (!needsLowSwizzle && !needsHighSwizzle) {
			outBuffer += 4;
			continue;
		}

		unsigned int srcPosLow = posLow;
		unsigned int srcPosHigh = posHigh;

		if (needsLowSwizzle) {
			srcPosLow ^= 3;
		}

		if (needsHighSwizzle) {
			srcPosHigh ^= 3;
		}

		unsigned int srcPos = (srcPosHigh << 6) + srcPosLow;

		if (srcPos >= blockCount) {
			outBuffer += 4;
			continue;
		}

		uint32_t* srcPtr = baseBuffer + (srcPos * 4);

		uint32_t data0 = srcPtr[0];
		uint32_t data1 = srcPtr[1];
		uint32_t data2 = srcPtr[2];
		uint32_t data3 = srcPtr[3];

		if (needsLowSwizzle) {
			uint32_t temp0 = data0;
			uint32_t t1 = (temp0 >> 8) & 0x0F000F0;
			uint32_t t2 = temp0 & 0x0F000F00;
			uint32_t t3 = t1 | t2;

			uint32_t t4 = temp0 & 0xFFFF000F;
			uint32_t t5 = temp0 & 0x0F000F0;
			uint32_t t6 = (t4 << 8) | t5;

			data0 = ((t3 >> 4) | (t6 << 4));

			temp0 = data0;
			t1 = (temp0 >> 8) & 0x0F000F0;
			t2 = temp0 & 0x0F000F00;
			t3 = t1 | t2;

			t4 = temp0 & 0xFFFF000F;
			t5 = temp0 & 0x0F000F0;
			t6 = (t4 << 8) | t5;

			data0 = ((t3 >> 4) | (t6 << 4));

			uint32_t temp3 = data3;
			uint32_t t7 = temp3 & 0xFF030303;
			uint32_t t8 = temp3 & 0x0C0C0C0C;
			uint32_t t9 = (t7 << 4) | t8;

			uint32_t t10 = (temp3 >> 4) & 0x0C0C0C0C;
			uint32_t t11 = temp3 & 0x30303030;
			uint32_t t12 = t10 | t11;

			data3 = ((t9 << 2) | (t12 >> 2));
		}

		if (needsHighSwizzle) {
			uint32_t temp0 = data0;
			data0 = (data1 >> 16) | (data1 << 16);
			data1 = (temp0 >> 16) | (temp0 << 16);

			uint32_t temp3 = data3;
			uint32_t t1 = temp3 & 0x00FF0000;
			uint32_t t2 = temp3 >> 16;
			uint32_t t3 = t1 | t2;

			uint32_t t4 = temp3 << 16;
			uint32_t t5 = temp3 & 0x0000FF00;
			uint32_t t6 = t4 | t5;

			data3 = ((t3 >> 8) | (t6 << 8));
		}

		outBuffer[0] = data0;
		outBuffer[1] = data1;
		outBuffer[2] = data2;
		outBuffer[3] = data3;

		outBuffer += 4;
	}
}