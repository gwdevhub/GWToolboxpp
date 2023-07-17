#include "stdafx.h"

#include "Event.h"

Event::Event(bool ManualReset, bool InitialState, const char* Name)
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

void Event::Pulse()
{
    PulseEvent(m_Handle);
}

void Event::SetDone()
{
    SetEvent(m_Handle);
}

void Event::Reset()
{
    ResetEvent(m_Handle);
}

void Event::WaitUntilDone()
{
    WaitForSingleObject(m_Handle, INFINITE);
}

bool Event::WaitWithTimeout(uint32_t WaitMs)
{
    return WaitForSingleObject(m_Handle, WaitMs) == WAIT_OBJECT_0;
}

bool Event::TryWait()
{
    return WaitForSingleObject(m_Handle, 0) == WAIT_OBJECT_0;
}

void Event::WaitAny(Event *Events, size_t Count, Event **EventSignaled)
{
    WaitAnyWithTimeout(Events, Count, INFINITE, EventSignaled);
}

void Event::WaitAll(Event *Events, size_t Count)
{
    WaitAllWithTimeout(Events, Count, INFINITE);
}

bool Event::WaitAnyWithTimeout(Event *Events, size_t Count, size_t WaitMs, Event **EventSignaled)
{
    assert(Count != 0);
    assert(Count <= MAXIMUM_WAIT_OBJECTS);

    HANDLE EventHandles[MAXIMUM_WAIT_OBJECTS];
    for (size_t i = 0; i < Count; ++i)
        EventHandles[i] = Events[i].m_Handle;

    DWORD dwResult = WaitForMultipleObjects(
        static_cast<DWORD>(Count),
        EventHandles,
        FALSE,
        WaitMs);

    bool Success = dwResult >= WAIT_OBJECT_0 && dwResult < (WAIT_OBJECT_0 + Count);
    if (Success && EventSignaled)
        *EventSignaled = &Events[dwResult - WAIT_OBJECT_0];

    return Success;
}

bool Event::WaitAllWithTimeout(Event *Events, size_t Count, size_t WaitMs)
{
    assert(Count != 0);
    assert(Count <= MAXIMUM_WAIT_OBJECTS);

    HANDLE EventHandles[MAXIMUM_WAIT_OBJECTS];
    for (size_t i = 0; i < Count; ++i)
        EventHandles[i] = Events[i].m_Handle;

    DWORD dwResult = WaitForMultipleObjects(
        Count,
        EventHandles,
        TRUE,
        WaitMs);

    return dwResult != 0;
}

bool Event::TryWaitAny(Event *Events, size_t Count, Event **EventSignaled)
{
    return WaitAnyWithTimeout(Events, Count, 0, EventSignaled);
}

bool Event::TryWaitAll(Event *Events, size_t Count)
{
    return WaitAllWithTimeout(Events, Count, 0);
}
