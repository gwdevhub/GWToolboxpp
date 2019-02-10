#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

template <typename T>
struct CircularBuffer {

    CircularBuffer(size_t size) {
        buffer = new T[size];
        allocated = size;
        count = 0;
        cursor = 0;
    }

    CircularBuffer() = default;

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
        assert(index < count);
        assert(cursor == count || count == allocated);
        size_t first = cursor % count;
        size_t i = (first + index) % count;
        return buffer[i];
    }

private:
    T     *buffer = nullptr;
    size_t cursor = 0; // next pos to write
    size_t count = 0;  // number of elements
    size_t allocated = 0;
};

#endif // CIRCULAR_BUFFER_H