#include "stdafx.h"

#include "DllVersion.h"

std::string PeFileVersion(const std::filesystem::path& file)
{
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return {};
    }

    const std::wstring filename = file.wstring();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(filename.c_str(), &handle);
    if (size == 0) {
        return {};
    }

    std::vector<char> buffer(size);
    if (!GetFileVersionInfoW(filename.c_str(), handle, size, buffer.data())) {
        return {};
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT info_size = sizeof(VS_FIXEDFILEINFO);
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &info_size) || info == nullptr) {
        return {};
    }

    return std::format("{}.{}.{}.{}", info->dwProductVersionMS >> 16, info->dwFileVersionMS & 0xFFFF, info->dwFileVersionLS >> 16, info->dwFileVersionLS & 0xFFFF);
}
