#include "Structures.h"

template <typename T>
DWORD GWAPI::gw_array<T>::size()
{
	return m_sizeCurrent;
}

template <typename T>
T GWAPI::gw_array<T>::operator[](DWORD index)
{
	return GetIndex(index);
}

template <typename T>
T GWAPI::gw_array<T>::GetIndex(DWORD index)
{
	if (index < 0 || index > m_sizeCurrent) return NULL;
	return m_array[index];
}

GWAPI::Agent* GWAPI::AgentArray::GetTarget()
{
	if (m_sizeCurrent == 0) return NULL;
	return m_array[GetTargetId()];
}

GWAPI::Agent* GWAPI::AgentArray::GetPlayer()
{
	if (m_sizeCurrent == 0) return NULL;
	return m_array[GetPlayerId()];
}

DWORD GWAPI::AgentArray::GetTargetId()
{
	return *(MemoryMgr::GetInstance()->TargetAgentIDPtr);
}

DWORD GWAPI::AgentArray::GetPlayerId()
{
	return *(MemoryMgr::GetInstance()->PlayerAgentIDPtr);
}
