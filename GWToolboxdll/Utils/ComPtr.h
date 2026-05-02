#pragma once

template<typename T>
class TBComPtr {
    T* ptr = nullptr;

public:
    TBComPtr() = default;
    ~TBComPtr() { if (ptr) ptr->Release(); }

    TBComPtr(const TBComPtr& o) : ptr(o.ptr) { if (ptr) ptr->AddRef(); }

    TBComPtr& operator=(const TBComPtr& o)
    {
        if (ptr != o.ptr) {
            if (ptr) ptr->Release();
            ptr = o.ptr;
            if (ptr) ptr->AddRef();
        }
        return *this;
    }

    TBComPtr(TBComPtr&& o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }

    TBComPtr& operator=(TBComPtr&& o) noexcept
    {
        if (this != &o) {
            if (ptr) ptr->Release();
            ptr = o.ptr;
            o.ptr = nullptr;
        }
        return *this;
    }

    // Takes ownership of p without calling AddRef.
    void Attach(T* p) { if (ptr) ptr->Release(); ptr = p; }
    void Reset() { Attach(nullptr); }
    T* Get() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};
