#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace AsyncStringDecoder {
    using Completion = std::function<void(const wchar_t*)>;

    void Decode(std::wstring_view encoded, Completion completion);
    [[nodiscard]] size_t PendingCount();
}
