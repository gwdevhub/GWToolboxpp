#pragma once

#include <Windows.h>

namespace GWAPI {

	// v1 hooker by 4D 1

	class Hook {
		BYTE* m_retourfunc;
		BYTE* m_source;
		DWORD m_length;
	public:
	
		BYTE* Detour(BYTE* _source, BYTE* _detour,const DWORD _length);
		void Retour();
		~Hook(){ if (m_source) Retour(); }
	};

}