#pragma once

// Windows-safe atomic UTF-8 JSON file publish. No GWCA / ImGui.

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress::AtomicJson {

struct WriteResult {
    bool ok = false;
    bool used_move_file_ex = false;
    bool used_replace_file = false;
    unsigned long win_error = 0;
    std::string message;
};

struct ReadResult {
    bool ok = false;
    bool missing = false;
    bool empty = false;
    unsigned long win_error = 0;
    std::string message;
    std::string utf8;
};

// Read entire file as bytes interpreted as UTF-8 (no validation beyond size).
ReadResult ReadFileUtf8(const std::filesystem::path& path);

// Write utf8 to same-directory .tmp, FlushFileBuffers, close, then:
// - if primary missing: MoveFileExW(tmp, primary, MOVEFILE_WRITE_THROUGH)
// - if primary exists: ReplaceFileW(primary, tmp, bak, ...)
// Never truncates primary in place. Does not delete primary/.bak on failure.
// Stale .tmp is overwritten at the start of a write; leftover .tmp after failure is left for diagnosis.
WriteResult WriteAtomicUtf8(
    const std::filesystem::path& primary,
    const std::filesystem::path& backup,
    std::string_view utf8);

} // namespace QuestProgress::AtomicJson
