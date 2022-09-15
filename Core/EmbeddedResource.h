#pragma once

class EmbeddedResource {
public:
    struct Parameters {
        std::size_t size_bytes = 0;
        void* ptr = nullptr;
    };

private:
    HRSRC hResource = nullptr;
    HGLOBAL hMemory = nullptr;

    Parameters p;

public:
    EmbeddedResource(int resource_id, const std::wstring& resource_class = L"RCDATA", HMODULE module = nullptr) {
        hResource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_class.c_str());
        if (!hResource) return;
        hMemory = LoadResource(module, hResource);
        if (!hMemory) return;

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    EmbeddedResource(int resource_id, const std::string& resource_class = "RCDATA", HMODULE module = nullptr) {
        hResource = FindResourceA(module, MAKEINTRESOURCEA(resource_id), resource_class.c_str());
        if (!hResource) return;
        hMemory = LoadResource(module, hResource);
        if (!hMemory) return;

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    EmbeddedResource(LPCWSTR resource_id, const std::wstring& resource_class = L"RCDATA", HMODULE module = nullptr) {
        hResource = FindResourceW(module, resource_id, resource_class.c_str());
        if (!hResource) return;
        hMemory = LoadResource(module, hResource);
        if (!hMemory) return;

        p.size_bytes = SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    EmbeddedResource(LPCSTR resource_id, const std::string& resource_class = "RCDATA", HMODULE module = nullptr) {
        hResource = FindResourceA(module, resource_id, resource_class.c_str());
        if (!hResource) return;
        hMemory = LoadResource(module, hResource);
        if (!hMemory) return;

        p.size_bytes = ::SizeofResource(module, hResource);
        p.ptr = LockResource(hMemory);
    }

    [[nodiscard]] auto data() const {
        return p.ptr;
    }

    [[nodiscard]] auto size() const {
        return p.size_bytes;
    }
};
