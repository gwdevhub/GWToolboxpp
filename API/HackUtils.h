#pragma once

#include <Windows.h>
#include <Psapi.h>

/*
	Scanner.h
	Credits to Zat & Midi12 @ unknowncheats.me for the functionality of this class.
*/

class CScanner{
	DWORD m_start;
	DWORD m_size;


public:

	CScanner(DWORD _start, DWORD _size) : m_start(_start), m_size(_size) {}

	CScanner(char* moduleName = NULL)
	{
		MODULEINFO info;
		if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(moduleName), &info, sizeof(MODULEINFO)))
			throw 1;

		m_start = (DWORD)info.lpBaseOfDll;
		m_size = (DWORD)info.SizeOfImage;

	}

	DWORD FindPattern(char* pattern, char* mask, DWORD offset)
	{
		int patternLength = strlen(mask);
		bool found = false;

		//For each byte from start to end
		for (DWORD i = m_start; i < m_start + m_size - patternLength; i++)
		{
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


};

class CHooker{
public:
	static void *Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore = NULL)
	{

		if (restore){
			*restore = new BYTE[len];
			memcpy(*restore, src, len);
		}


		BYTE *jmp = (BYTE*)malloc(len + 5);
		DWORD dwBack;

		VirtualProtect(src, len, PAGE_READWRITE, &dwBack);

		memcpy(jmp, src, len);
		jmp += len;

		jmp[0] = 0xE9;
		*(DWORD*)(jmp + 1) = (DWORD)(src + len - jmp) - 5;

		src[0] = 0xE9;
		*(DWORD*)(src + 1) = (DWORD)(dst - src) - 5;

		for (int i = 5; i < len; i++)
			src[i] = 0x90;

		VirtualProtect(src, len, dwBack, &dwBack);

		return (jmp - len);
	}

	static void Retour(BYTE *src, BYTE *restore, const int len)
	{
		DWORD dwBack;

		VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
		memcpy(src, restore, len);

		restore[0] = 0xE9;
		*(DWORD*)(restore + 1) = (DWORD)(src - restore) - 5;

		VirtualProtect(src, len, dwBack, &dwBack);

		delete[] restore;
	}

};

class CMemory{

template <typename T>
static T ReadPtrChain(DWORD BasePointer, BYTE numOfOffsets, ...)
{
	va_list vl;
	va_start(vl, numOfOffsets);
	while (numOfOffsets--)
	{
		BasePointer = *(DWORD*)(BasePointer + va_arg(vl, DWORD));
	}
	va_end(vl);
	return (T)BasePointer;
}

};