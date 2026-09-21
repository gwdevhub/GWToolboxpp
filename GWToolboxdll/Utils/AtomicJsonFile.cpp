#include <Utils/AtomicJsonFile.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <fstream>
#include <string>

namespace QuestProgress::AtomicJson {
namespace {

std::wstring Native(const std::filesystem::path& path)
{
    return path.wstring();
}

std::string WinErrorMessage(DWORD err)
{
    if (err == 0) {
        return {};
    }
    char buf[256]{};
    const auto n = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        0,
        buf,
        static_cast<DWORD>(sizeof(buf)),
        nullptr);
    if (n == 0) {
        return "win_error=" + std::to_string(err);
    }
    // Trim trailing CR/LF.
    std::string msg(buf, buf + n);
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ')) {
        msg.pop_back();
    }
    return msg + " (" + std::to_string(err) + ")";
}

} // namespace

ReadResult ReadFileUtf8(const std::filesystem::path& path)
{
    ReadResult out;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        out.missing = true;
        out.ok = true;
        out.message = "file missing";
        return out;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        out.win_error = static_cast<unsigned long>(ec.value());
        out.message = "file_size failed: " + ec.message();
        return out;
    }
    if (size == 0) {
        out.empty = true;
        out.ok = true;
        out.message = "file empty";
        return out;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        out.message = "open for read failed";
        return out;
    }
    out.utf8.assign(static_cast<size_t>(size), '\0');
    file.read(out.utf8.data(), static_cast<std::streamsize>(size));
    if (!file) {
        out.utf8.clear();
        out.message = "read failed";
        return out;
    }
    out.ok = true;
    return out;
}

WriteResult WriteAtomicUtf8(
    const std::filesystem::path& primary,
    const std::filesystem::path& backup,
    std::string_view utf8)
{
    WriteResult out;
    std::error_code ec;
    const auto parent = primary.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            out.message = "create_directories failed: " + ec.message();
            return out;
        }
    }

    const auto tmp = primary.wstring() + L".tmp";
    const auto primary_w = Native(primary);
    const auto backup_w = Native(backup);

    // Overwrite any stale tmp from a prior crashed write (safe: tmp is never the durable primary).
    const HANDLE file = CreateFileW(
        tmp.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        out.win_error = GetLastError();
        out.message = "CreateFileW tmp failed: " + WinErrorMessage(out.win_error);
        return out;
    }

    DWORD written = 0;
    const auto* data = reinterpret_cast<const BYTE*>(utf8.data());
    auto remaining = static_cast<DWORD>(utf8.size());
    auto offset = 0u;
    while (remaining > 0) {
        if (!WriteFile(file, data + offset, remaining, &written, nullptr)) {
            out.win_error = GetLastError();
            out.message = "WriteFile tmp failed: " + WinErrorMessage(out.win_error);
            CloseHandle(file);
            return out;
        }
        offset += written;
        remaining -= written;
    }

    if (!FlushFileBuffers(file)) {
        out.win_error = GetLastError();
        out.message = "FlushFileBuffers failed: " + WinErrorMessage(out.win_error);
        CloseHandle(file);
        return out;
    }
    CloseHandle(file);

    const bool primary_exists = std::filesystem::exists(primary, ec) && !ec;
    if (!primary_exists) {
        if (!MoveFileExW(tmp.c_str(), primary_w.c_str(), MOVEFILE_WRITE_THROUGH)) {
            out.win_error = GetLastError();
            out.message = "MoveFileExW first-create failed: " + WinErrorMessage(out.win_error);
            return out;
        }
        out.used_move_file_ex = true;
        out.ok = true;
        out.message = "created via MoveFileExW";
        return out;
    }

    // REPLACEFILE_WRITE_THROUGH = 0x00000002
    if (!ReplaceFileW(
            primary_w.c_str(),
            tmp.c_str(),
            backup_w.c_str(),
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr)) {
        out.win_error = GetLastError();
        out.message = "ReplaceFileW failed: " + WinErrorMessage(out.win_error);
        // Leave primary and bak untouched; tmp may remain for diagnosis.
        return out;
    }
    out.used_replace_file = true;
    out.ok = true;
    out.message = "replaced via ReplaceFileW";
    return out;
}

} // namespace QuestProgress::AtomicJson
