#include "AsyncStringDecoder.h"

#include <GWCA/Managers/UIMgr.h>

#include <atomic>
#include <utility>

namespace {
    struct DecodeContext {
        std::wstring encoded;
        AsyncStringDecoder::Completion completion;
    };

    std::atomic_size_t pendingDecodes = 0;

    void OnDecoded(void* rawContext, const wchar_t* decoded)
    {
        const auto context = static_cast<DecodeContext*>(rawContext);
        try {
            context->completion(decoded);
        }
        catch (...) {
            // Completion failures must not strand plugin shutdown.
        }
        delete context;
        --pendingDecodes;
    }
}

void AsyncStringDecoder::Decode(const std::wstring_view encoded, Completion completion)
{
    const auto context = new DecodeContext{std::wstring(encoded), std::move(completion)};
    ++pendingDecodes;
    GW::UI::AsyncDecodeStr(context->encoded.c_str(), &OnDecoded, context);
}

size_t AsyncStringDecoder::PendingCount()
{
    return pendingDecodes.load();
}
