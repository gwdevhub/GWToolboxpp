// One heap, the game's -- a second allocator over the same linear memory corrupts it immediately. See README.md.

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

    // Supplied by the loader from the game's exports; explicit imports so a missing one fails at instantiation by name.
    __attribute__((import_module("env"), import_name("host_malloc")))
    void* host_malloc(size_t size);

    __attribute__((import_module("env"), import_name("host_free")))
    void host_free(void* ptr);

    __attribute__((import_module("env"), import_name("host_realloc")))
    void* host_realloc(void* ptr, size_t size);

    void* malloc(size_t size)
    {
        return host_malloc(size);
    }

    void free(void* ptr)
    {
        host_free(ptr);
    }

    void* realloc(void* ptr, size_t size)
    {
        return host_realloc(ptr, size);
    }

    void* calloc(size_t count, size_t size)
    {
        // Overflow matters: counts come from game data, and a wrapped product allocates short then gets memset past its end.
        const size_t total = count * size;
        if (count && total / count != size) return nullptr;
        void* p = host_malloc(total);
        if (p) memset(p, 0, total);
        return p;
    }

    // libc reaches its allocator through these aliases, and -sMALLOC=none leaves them undefined -- define them here.
    void* __libc_malloc(size_t size) { return host_malloc(size); }
    void  __libc_free(void* ptr) { host_free(ptr); }
    void* __libc_calloc(size_t count, size_t size) { return calloc(count, size); }
    void* __libc_realloc(void* ptr, size_t size) { return host_realloc(ptr, size); }
    void* emscripten_builtin_malloc(size_t size) { return host_malloc(size); }
    void  emscripten_builtin_free(void* ptr) { host_free(ptr); }
    void* emscripten_builtin_calloc(size_t count, size_t size) { return calloc(count, size); }
    void* emscripten_builtin_realloc(void* ptr, size_t size) { return host_realloc(ptr, size); }

    // libc++ reaches for these on some paths; Emscripten's dlmalloc would have supplied them.
    void* aligned_alloc(size_t alignment, size_t size)
    {
        // The game's allocator returns 8- or 16-byte aligned blocks; refuse anything stricter rather than return misaligned.
        if (alignment > 16) return nullptr;
        return host_malloc(size);
    }

    int posix_memalign(void** out, size_t alignment, size_t size)
    {
        if (!out) return 22;                      // EINVAL
        void* p = aligned_alloc(alignment, size);
        *out = p;
        return p ? 0 : 12;                        // ENOMEM
    }

}  // extern "C"
