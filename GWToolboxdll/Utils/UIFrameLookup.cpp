#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/UIFrameLookup.h>
#include <Logger.h>

namespace {
    // Per spec for TextMultiline_UICallback: message id 9 = frame created, 8 = frame destroyed.
    constexpr uint32_t MessageFrameCreated = 0x9;
    constexpr uint32_t MessageFrameDestroyed = 0x8;

    // encoded label -> live multiline text frame, kept current by the callback below.
    std::unordered_map<std::wstring, GW::MultiLineTextLabelFrame*> multiline_frames;

    GW::UI::UIInteractionCallback TextMultiline_UICallback_Func = nullptr;
    GW::UI::UIInteractionCallback TextMultiline_UICallback_Ret = nullptr;

    void OnTextMultilineUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        // Drop the entry before the original handler tears the frame down.
        if (static_cast<uint32_t>(message->message_id) == MessageFrameDestroyed) {
            const auto frame = GW::UI::GetFrameById(message->frame_id);
            if (frame) {
                std::erase_if(multiline_frames, [frame](const auto& entry) { return entry.second == frame; });
            }
        }
        TextMultiline_UICallback_Ret(message, wparam, lparam);
        // Read the label after creation, once the frame is initialised.
        if (static_cast<uint32_t>(message->message_id) == MessageFrameCreated) {
            const auto frame = static_cast<GW::MultiLineTextLabelFrame*>(GW::UI::GetFrameById(message->frame_id));
            const auto label = frame ? frame->GetEncodedLabel() : nullptr;
            if (label && label[0]) {
                multiline_frames[label] = frame;
            }
        }
        GW::Hook::LeaveHook();
    }

    bool EnsureInitialised()
    {
        static bool initialised = false;
        if (initialised) {
            return TextMultiline_UICallback_Func != nullptr;
        }
        initialised = true;
        TextMultiline_UICallback_Func = reinterpret_cast<GW::UI::UIInteractionCallback>(
            GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x83\xc0\xf8\x83\xf8\x5b", "xxxxxx")));
        if (!TextMultiline_UICallback_Func) {
            Log::Error("Failed to find TextMultiline_UICallback");
            return false;
        }
        GW::Hook::CreateHook(reinterpret_cast<void**>(&TextMultiline_UICallback_Func), OnTextMultilineUICallback,
                             reinterpret_cast<void**>(&TextMultiline_UICallback_Ret));
        GW::Hook::EnableHooks(TextMultiline_UICallback_Func);
        return true;
    }
}

namespace GW::UI {
    GW::MultiLineTextLabelFrame* GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        if (!encoded_string || !EnsureInitialised()) {
            return nullptr;
        }
        if (!partial_match) {
            const auto found = multiline_frames.find(encoded_string);
            return found == multiline_frames.end() ? nullptr : found->second;
        }
        for (const auto& [label, frame] : multiline_frames) {
            if (label.starts_with(encoded_string)) {
                return frame;
            }
        }
        return nullptr;
    }
}
