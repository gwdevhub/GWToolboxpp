#pragma once

class Event {
public:
    static constexpr size_t MAX_WAIT_OBJECTS = 64;

public:
    Event(bool ManualReset, bool InitialState, const char* Name = nullptr);
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    ~Event();

    // Releases 1 waiting thread if auto-resetting, otherwise all of them;
    // either way the event is reset after this call.
    void Pulse() const;

    // If the event is created with the auto-reset flag enabled, only one
    // thread is released, and the event is then reset.
    void SetDone() const;

    // Threads waiting until the event is done block until SetDone() or Pulse()
    // is called. This function is only useful for events that are not auto-resetting.
    void Reset() const;

    void WaitUntilDone() const;

    bool WaitWithTimeout(uint32_t WaitMs) const;

    bool TryWait() const;

    // The static wait helpers below take at most "MAX_WAIT_OBJECTS" events.
    static void WaitAny(Event* Events, size_t Count, Event** EventSignaled);

    static void WaitAll(Event* Events, size_t Count);

    static bool WaitAnyWithTimeout(Event* Events, size_t Count, uint32_t WaitMs, Event** EventSignaled);

    static bool WaitAllWithTimeout(const Event* Events, size_t Count, uint32_t WaitMs);

    static bool TryWaitAny(Event* Events, size_t Count, Event** EventSignaled);

    static bool TryWaitAll(Event* Events, size_t Count);

private:
    void* m_Handle;
};
