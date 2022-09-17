#pragma once

template <typename T>
struct CircularBuffer {

    CircularBuffer(size_t size)
        : buffer(new T[size])
        , allocated(size)
    {
    }

    CircularBuffer() = default;

    CircularBuffer(const CircularBuffer&) = delete;

    CircularBuffer(CircularBuffer&& other) noexcept
        : buffer(other.buffer)
        , cursor(other.cursor)
        , count(other.count)
        , allocated(other.allocated)
    {
        other.buffer = nullptr;
    }

    ~CircularBuffer() noexcept {
        delete[] buffer;
    }

    bool full()   { return size == allocated; }
    void clear()  { count = 0, cursor = 0; }
    size_t size() { return count; }

    void add(const T &val) {
        buffer[cursor] = val;
        cursor = (cursor + 1) % allocated;
        if (count < allocated) {
            count++;
        }
    }

    T& operator[](size_t index) {
        ASSERT(index < count);
        ASSERT(cursor == count || count == allocated);
        size_t first = cursor % count;
        size_t i = (first + index) % count;
        return buffer[i];
    }

    CircularBuffer& operator=(CircularBuffer&& other) noexcept {
        this->~CircularBuffer();
        buffer = other.buffer;
        cursor = other.cursor;
        count = other.count;
        allocated = other.allocated;
        other.buffer = nullptr;
        return *this;
    }

private:
    T     *buffer = nullptr;
    size_t cursor = 0; // next pos to write
    size_t count = 0;  // number of elements
    size_t allocated = 0;
};
