#pragma once

class Event
{
public:
    static const size_t MAX_WAIT_OBJECTS = 64;

public:
    Event(bool ManualReset, bool InitialState, const char* Name = nullptr);
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    ~Event();

    // If the event is auto resetting only 1 thread is released. Otherwise
    // all waiting threads are released. In both cases the event is reset
    // after this call.
    void Pulse();

    // If the event is created with the auto-reset flag enabled, only one
    // thread is released, and the event is then reset.
    void SetDone();

    // Threads waiting until the event is done block until SetDone() or Pulse()
    // is called. This function is only useful for events that are not auto-resetting.
    void Reset();

    // Blocks until the event is set to done.
    void WaitUntilDone();

    // Returns True if event is set done. False otherwise.
    bool WaitWithTimeout(uint32_t WaitMs);

    // True if the event is set done. False otherwise.
    bool TryWait();

    // Blocks until at least one event is set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static void WaitAny(Event *Events, size_t Count, Event **EventSignaled);

    // Blocks until all events are set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static void WaitAll(Event *Events, size_t Count);

    // Wait up to the requested msecs for at least one event to be set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static bool WaitAnyWithTimeout(Event *Events, size_t Count, uint32_t WaitMs, Event **EventSignaled);

    // Wait up to the requested msecs for all events to be set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static bool WaitAllWithTimeout(Event *Events, size_t Count, uint32_t WaitMs);

    //  Check if at least one event is set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static bool TryWaitAny(Event *Events, size_t Count, Event **EventSignaled);

    // Check if all events are set to done.
    // You can wait on most "MAX_WAIT_OBJECTS".
    static bool TryWaitAll(Event *Events, size_t Count);

private:
    void *m_Handle;
};
