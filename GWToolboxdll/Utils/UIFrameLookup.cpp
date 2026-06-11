#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/UIFrameLookup.h>
#include <Logger.h>

namespace {
    // encoded label -> live multiline text frame, kept current by the callback below.
    std::unordered_map<std::wstring, GW::MultiLineTextLabelFrame*> multiline_frames;

    GW::UI::UIInteractionCallback TextMultiline_UICallback_Func = nullptr;
    GW::UI::UIInteractionCallback TextMultiline_UICallback_Ret = nullptr;

    void ForgetFrame(const GW::UI::Frame* frame)
    {
        std::erase_if(multiline_frames, [frame](const auto& entry) { return entry.second == frame; });
    }

    void OnTextMultilineUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        // Drop the entry before the original handler tears the frame down.
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) {
            if (const auto frame = GW::UI::GetFrameById(message->frame_id)) {
                ForgetFrame(frame);
            }
        }
        TextMultiline_UICallback_Ret(message, wparam, lparam);
        switch (message->message_id) {
            // Frame created; cache its initial encoded label.
            case GW::UI::UIMessage::kInitFrame: {
                const auto frame = static_cast<GW::MultiLineTextLabelFrame*>(GW::UI::GetFrameById(message->frame_id));
                const auto label = frame ? frame->GetEncodedLabel() : nullptr;
                if (label && label[0]) {
                    multiline_frames[label] = frame;
                }
            } break;
            // Encoded label is being reassigned; wparam is the new encoded string.
            case GW::UI::UIMessage::kFrameMessage_0x52: {
                const auto frame = static_cast<GW::MultiLineTextLabelFrame*>(GW::UI::GetFrameById(message->frame_id));
                if (frame) {
                    ForgetFrame(frame);
                    const auto new_label = static_cast<const wchar_t*>(wparam);
                    if (new_label && new_label[0]) {
                        multiline_frames[new_label] = frame;
                    }
                }
            } break;
            default:
                break;
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
