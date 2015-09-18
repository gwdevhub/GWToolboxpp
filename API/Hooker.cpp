#include "Hooker.h"

void GWAPI::Hook::Retour()
{
	DWORD dwOldProt;

	VirtualProtect(m_source, m_length, PAGE_READWRITE, &dwOldProt);

	memcpy(m_source, m_retourfunc, m_length);

	VirtualProtect(m_source, m_length, dwOldProt, &dwOldProt);

	delete[] m_retourfunc;
}

BYTE* GWAPI::Hook::Detour(BYTE* _source, BYTE* _detour, const DWORD _length)
{
	DWORD dwOldProt;

	m_source = _source;
	m_length = _length;
	m_retourfunc = new BYTE[m_length + 5];

	memcpy(m_retourfunc, m_source, m_length);

	(m_retourfunc + m_length)[0] = 0xE9;
	*(DWORD*)(m_retourfunc + m_length + 1) = (DWORD)((m_source + m_length) - (m_retourfunc + m_length + 5));

	VirtualProtect(m_source, m_length, PAGE_READWRITE, &dwOldProt);

	m_source[0] = 0xE9;
	*(DWORD*)(m_source + 1) = (DWORD)(_detour - (m_source + 5));

	if (m_length != 5)
		for (DWORD i = 5; i < m_length;i++)
			m_source[i] = 0x90;

	VirtualProtect(m_source, m_length, dwOldProt, &dwOldProt);


	return m_retourfunc;
}
