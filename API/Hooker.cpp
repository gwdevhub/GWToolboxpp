#include "Hooker.h"

extern "C"{
#include "disasm/ld32.h"
}

void GWAPI::Hook::Retour()
{
	DWORD old_protection;

	VirtualProtect(source_, length_, PAGE_READWRITE, &old_protection);

	memcpy(source_, retour_func_, length_);

	VirtualProtect(source_, length_, old_protection, &old_protection);

	delete[] retour_func_;
}

BYTE* GWAPI::Hook::Detour(BYTE* _source, BYTE* _detour, const DWORD _length)
{
	DWORD old_protection;

	source_ = _source;
	length_ = _length;
	retour_func_ = new BYTE[length_ + 5];

	memcpy(retour_func_, source_, length_);

	retour_func_ += length_;
	
	retour_func_[0] = 0xE9;
	*(DWORD*)(retour_func_ + 1) = (DWORD)((source_ + length_) - (retour_func_ + 5));

	VirtualProtect(source_, length_, PAGE_READWRITE, &old_protection);

	source_[0] = 0xE9;
	*(DWORD*)(source_ + 1) = (DWORD)(_detour - (source_ + 5));

	if (length_ != 5)
		for (DWORD i = 5; i < length_;i++)
			source_[i] = 0x90;

	VirtualProtect(source_, length_, old_protection, &old_protection);

	retour_func_ -= length_;

	return retour_func_;
}

DWORD GWAPI::Hook::CalculateDetourLength(BYTE* _source)
{
	
	DWORD len = 0;
	DWORD current_op;

	do{
		current_op = length_disasm((void*)_source);
		if (current_op != 0){
			len += current_op;
			_source += current_op;
		}
	} while (len < 5);

	return len;
}
