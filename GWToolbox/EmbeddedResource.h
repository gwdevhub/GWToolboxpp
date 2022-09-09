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
    EmbeddedResource(int resource_id, const std::wstring& resource_class) {
        hResource = FindResourceW(nullptr, MAKEINTRESOURCEW(resource_id), resource_class.c_str());
        hMemory = ::LoadResource(nullptr, hResource);

        p.size_bytes = ::SizeofResource(nullptr, hResource);
        p.ptr = ::LockResource(hMemory);
    }

    auto GetResourceData() const {
        return p.ptr;
    }

    auto GetResourceSize() const {
        return p.size_bytes;
    }
};