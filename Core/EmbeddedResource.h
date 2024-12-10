#pragma once

class EmbeddedResource {
public:
    struct Parameters {
        size_t size_bytes = 0;
        void* ptr = nullptr;
    };

private:
    HRSRC hResource = nullptr;
    HGLOBAL hMemory = nullptr;

    Parameters p;

public:
    EmbeddedResource(const int resource_id, const std::string_view resource_class = "RCDATA", const HMODULE module = nullptr)
    {
        hResource = FindResourceA(module, MAKEINTRESOURCEA(resource_id), resource_class.data());
        if (!hResource) {
            DWORD error = GetLastError();
            Log::Error("FindResourceA failed with error: %d", error);
            return;
        }
        hMemory = LoadResource(module, hResource);
        if (!hMemory) {
            return;
        }

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    EmbeddedResource(const int resource_id, const LPCSTR resource_class = RT_RCDATA, const HMODULE module = nullptr)
    {
        hResource = FindResourceA(module, MAKEINTRESOURCEA(resource_id), resource_class);
        if (!hResource) {
            DWORD error = GetLastError();
            Log::Error("FindResourceA failed with error: %d", error);
            return;
        }
        hMemory = LoadResource(module, hResource);
        if (!hMemory) {
            return;
        }

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    EmbeddedResource(const LPCSTR resource_id, const LPCSTR resource_class = RT_RCDATA, const HMODULE module = nullptr)
    {
        hResource = FindResourceA(module, resource_id, resource_class);
        if (!hResource) {
            DWORD error = GetLastError();
            Log::Error("FindResourceA failed with error: %d", error);
            return;
        }
        hMemory = LoadResource(module, hResource);
        if (!hMemory) {
            return;
        }

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    [[nodiscard]] auto data() const
    {
        return p.ptr;
    }

    [[nodiscard]] auto size() const
    {
        return p.size_bytes;
    }
};
