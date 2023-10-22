#include "stdafx.h"

#include "Event.h"

Event::Event(const bool ManualReset, const bool InitialState, const char* Name)
{
    // @Enhancement: Ensure "Name" is ascii

    m_Handle = CreateEventA(nullptr, ManualReset, InitialState, Name);
    if (m_Handle == nullptr) {
        // @Enhancement: Add logging
        // LOG("Unable to create handle for Event");
    }
}

Event::~Event()
{
    CloseHandle(m_Handle);
}

void Event::Pulse() const
{
    PulseEvent(m_Handle);
}

void Event::SetDone() const
{
    SetEvent(m_Handle);
}

void Event::Reset() const
{
    ResetEvent(m_Handle);
}

void Event::WaitUntilDone() const
{
    WaitForSingleObject(m_Handle, INFINITE);
}

bool Event::WaitWithTimeout(const uint32_t WaitMs) const
{
    return WaitForSingleObject(m_Handle, WaitMs) == WAIT_OBJECT_0;
}

bool Event::TryWait() const
{
    return WaitForSingleObject(m_Handle, 0) == WAIT_OBJECT_0;
}

void Event::WaitAny(Event* Events, const size_t Count, Event** EventSignaled)
{
    WaitAnyWithTimeout(Events, Count, INFINITE, EventSignaled);
}

void Event::WaitAll(Event* Events, const size_t Count)
{
    WaitAllWithTimeout(Events, Count, INFINITE);
}

bool Event::WaitAnyWithTimeout(Event* Events, const size_t Count, const size_t WaitMs, Event** EventSignaled)
{
    assert(Count != 0);
    assert(Count <= MAXIMUM_WAIT_OBJECTS);

    HANDLE EventHandles[MAXIMUM_WAIT_OBJECTS];
    for (size_t i = 0; i < Count; ++i) {
        EventHandles[i] = Events[i].m_Handle;
    }

    const DWORD dwResult = WaitForMultipleObjects(
        Count,
        EventHandles,
        FALSE,
        WaitMs);

    const bool Success = dwResult >= WAIT_OBJECT_0 && dwResult < WAIT_OBJECT_0 + Count;
    if (Success && EventSignaled) {
        *EventSignaled = &Events[dwResult - WAIT_OBJECT_0];
    }

    return Success;
}

bool Event::WaitAllWithTimeout(const Event* Events, const size_t Count, const size_t WaitMs)
{
    assert(Count != 0);
    assert(Count <= MAXIMUM_WAIT_OBJECTS);

    HANDLE EventHandles[MAXIMUM_WAIT_OBJECTS];
    for (size_t i = 0; i < Count; ++i) {
        EventHandles[i] = Events[i].m_Handle;
    }

    const DWORD dwResult = WaitForMultipleObjects(
        Count,
        EventHandles,
        TRUE,
        WaitMs);

    return dwResult != 0;
}

bool Event::TryWaitAny(Event* Events, const size_t Count, Event** EventSignaled)
{
    return WaitAnyWithTimeout(Events, Count, 0, EventSignaled);
}

bool Event::TryWaitAll(Event* Events, const size_t Count)
{
    return WaitAllWithTimeout(Events, Count, 0);
}
