#pragma once

#include <Windows.h>


namespace GWAPI {

	// v1 hooker by 4D 1

	class Hook {
		BYTE* retour_func_;
		BYTE* source_;
		DWORD length_;
	public:
	
		static DWORD CalculateDetourLength(BYTE* _source);
		BYTE* Detour(BYTE* _source, BYTE* _detour,const DWORD _length);
		void Retour();
	};

}