#pragma once

#include <vector>
#include <functional>
#include <tuple>
#include <mutex>
#include <atomic>
#include <memory>

/*
	Code created and 
*/


class CallQueue{
	std::vector<std::function<void( void )> > m_Calls;
    mutable std::mutex m_CallVecMutex;
public:
	// For use only in gameloop hook.
	void __stdcall CallFunctions();

	// Add function to queue.
	template<typename F, typename... ArgTypes>
	void Enqueue( F&& Func, ArgTypes&&... Args );
	
	
};

template<typename F, typename... ArgTypes>
void CallQueue::Enqueue( F&& Func, ArgTypes&&... Args )
{
    std::unique_lock<std::mutex> VecLock( m_CallVecMutex );
    m_Calls.emplace_back( std::bind(Func,Args...) );
}

void __stdcall CallQueue::CallFunctions()
{
		
	if(m_Calls.empty()) return;
 
    std::unique_lock<std::mutex> VecLock( m_CallVecMutex );
    for (const auto& Call : m_Calls)
    {
	   printf("Calling %X", Call);
       Call();
    }
 
    m_Calls.clear();
}