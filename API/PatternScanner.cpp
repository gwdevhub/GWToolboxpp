#include "PatternScanner.h"
#include <Psapi.h>

DWORD GWAPI::PatternScanner::FindPattern(char* pattern, char* mask, DWORD offset)
{
	BYTE first = pattern[0];
	int patternLength = strlen(mask);
	bool found = false;

	//For each byte from start to end
	for (DWORD i = base_; i < base_ + size_ - patternLength; i++)
	{
		if (*(BYTE*)i != first)
		{
			continue;
		}
		found = true;
		//For each byte in the pattern
		for (int idx = 0; idx < patternLength; idx++)
		{

			if (mask[idx] == 'x' && pattern[idx] != *(char*)(i + idx))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			return i + offset;
		}
	}
	return NULL;
}

GWAPI::PatternScanner::PatternScanner(HMODULE _module)
{
	MODULEINFO info;
	if (!GetModuleInformation(GetCurrentProcess(), _module, &info, sizeof(MODULEINFO)))
		throw 1;

	base_ = (DWORD)info.lpBaseOfDll;
	size_ = (DWORD)info.SizeOfImage;
}

GWAPI::PatternScanner::PatternScanner(char* moduleName /*= NULL*/)
{
	MODULEINFO info;
	if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(moduleName), &info, sizeof(MODULEINFO)))
		throw 1;

	base_ = (DWORD)info.lpBaseOfDll;
	size_ = (DWORD)info.SizeOfImage;
}

GWAPI::PatternScanner::PatternScanner(DWORD _start, DWORD _size) : base_(_start), size_(_size)
{

}
