#pragma once

template <typename T>
struct CircularBuffer {
    CircularBuffer(const size_t size) : buffer(new T[size]), allocated(size) {}

    CircularBuffer() = default;

    CircularBuffer(const CircularBuffer&) = delete;

    CircularBuffer(CircularBuffer&& other) noexcept : buffer(other.buffer), cursor(other.cursor), count(other.count), allocated(other.allocated) { other.buffer = nullptr; }

    ~CircularBuffer() noexcept { delete[] buffer; }

    bool full() { return count == allocated; }
    void clear() { count = 0, cursor = 0; }
    size_t size() const { return count; }

    void add(const T& val)
    {
        buffer[cursor] = val;
        cursor = (cursor + 1) % allocated;
        if (count < allocated) {
            count++;
        }
    }

    T& operator[](const size_t index)
    {
        ASSERT(index < count);
        ASSERT(cursor == count || count == allocated);
        const size_t first = cursor >= count ? 0 : cursor - count + allocated;
        size_t i = (first + index) % allocated;
        return buffer[i];
    }

    CircularBuffer& operator=(CircularBuffer&& other) noexcept
    {
        this->~CircularBuffer();
        buffer = other.buffer;
        cursor = other.cursor;
        count = other.count;
        allocated = other.allocated;
        other.buffer = nullptr;
        return *this;
    }

    // Iterator class for range-based for loops
    class Iterator {
    public:
        Iterator(CircularBuffer* buf, size_t index) : buffer(buf), current_index(index) {}

        T& operator*() { return (*buffer)[current_index]; }

        T* operator->() { return &(*buffer)[current_index]; }

        Iterator& operator++()
        {
            ++current_index;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++current_index;
            return temp;
        }

        bool operator==(const Iterator& other) const { return buffer == other.buffer && current_index == other.current_index; }

        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        CircularBuffer* buffer;
        size_t current_index;
    };

    // Const iterator class
    class ConstIterator {
    public:
        ConstIterator(const CircularBuffer* buf, size_t index) : buffer(buf), current_index(index) {}

        const T& operator*() const { return (*const_cast<CircularBuffer*>(buffer))[current_index]; }

        const T* operator->() const { return &(*const_cast<CircularBuffer*>(buffer))[current_index]; }

        ConstIterator& operator++()
        {
            ++current_index;
            return *this;
        }

        ConstIterator operator++(int)
        {
            ConstIterator temp = *this;
            ++current_index;
            return temp;
        }

        bool operator==(const ConstIterator& other) const { return buffer == other.buffer && current_index == other.current_index; }

        bool operator!=(const ConstIterator& other) const { return !(*this == other); }

    private:
        const CircularBuffer* buffer;
        size_t current_index;
    };

    // Iterator access methods
    Iterator begin() { return Iterator(this, 0); }

    Iterator end() { return Iterator(this, count); }

    ConstIterator begin() const { return ConstIterator(this, 0); }

    ConstIterator end() const { return ConstIterator(this, count); }

    ConstIterator cbegin() const { return ConstIterator(this, 0); }

    ConstIterator cend() const { return ConstIterator(this, count); }

private:
    T* buffer = nullptr;
    size_t cursor = 0; // next pos to write
    size_t count = 0;  // number of elements
    size_t allocated = 0;
};
