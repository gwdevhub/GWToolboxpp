#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/UIFrameLookup.h>
#include <Logger.h>

namespace {
    // encoded label -> live frame, kept current by the callbacks below.
    std::unordered_map<std::wstring, GW::MultiLineTextLabelFrame*> multiline_frames;
    std::unordered_map<std::wstring, GW::ButtonFrame*> button_frames;

    GW::UI::UIInteractionCallback TextMultiline_UICallback_Func = nullptr;
    GW::UI::UIInteractionCallback TextMultiline_UICallback_Ret = nullptr;
    GW::UI::UIInteractionCallback CtlButton_UICallback_Func = nullptr;
    GW::UI::UIInteractionCallback CtlButton_UICallback_Ret = nullptr;

    const wchar_t* ReadEncodedLabel(GW::MultiLineTextLabelFrame* frame)
    {
        return frame ? frame->GetEncodedLabel() : nullptr;
    }
    const wchar_t* ReadEncodedLabel(GW::ButtonFrame* frame)
    {
        const wchar_t* label = nullptr;
        return frame && frame->GetLabel(&label) ? label : nullptr;
    }

    template <typename FrameT>
    void ForgetFrame(std::unordered_map<std::wstring, FrameT*>& cache, const GW::UI::Frame* frame)
    {
        std::erase_if(cache, [frame](const auto& entry) { return entry.second == frame; });
    }

    // Shared lifecycle for a frame-type cache: created/destroyed frames are tracked by their
    // encoded label, and relabel_message re-reads the frame's label to reassign the cache key.
    template <typename FrameT>
    void UpdateCache(std::unordered_map<std::wstring, FrameT*>& cache, GW::UI::UIInteractionCallback original,
                     GW::UI::UIMessage relabel_message, GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        // Drop the entry before the original handler tears the frame down.
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) {
            if (const auto frame = GW::UI::GetFrameById(message->frame_id)) {
                ForgetFrame(cache, frame);
            }
        }
        original(message, wparam, lparam);
        // Frame created or relabelled; re-key it under its current encoded label.
        if (message->message_id == GW::UI::UIMessage::kInitFrame || message->message_id == relabel_message) {
            const auto frame = static_cast<FrameT*>(GW::UI::GetFrameById(message->frame_id));
            if (frame) {
                ForgetFrame(cache, frame);
                const auto label = ReadEncodedLabel(frame);
                if (label && label[0]) {
                    cache[label] = frame;
                }
            }
        }
        GW::Hook::LeaveHook();
    }

    void OnTextMultilineUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        UpdateCache(multiline_frames, TextMultiline_UICallback_Ret, GW::UI::UIMessage::kFrameMessage_0x52, message, wparam, lparam);
    }
    void OnCtlButtonUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        UpdateCache(button_frames, CtlButton_UICallback_Ret, GW::UI::UIMessage::kFrameMessage_0x4c, message, wparam, lparam);
    }

    bool InstallHook(GW::UI::UIInteractionCallback& func, GW::UI::UIInteractionCallback detour, GW::UI::UIInteractionCallback& ret, uintptr_t address, const char* name)
    {
        func = reinterpret_cast<GW::UI::UIInteractionCallback>(address);
        if (!func) {
            Log::Error("Failed to find %s", name);
            return false;
        }
        GW::Hook::CreateHook(reinterpret_cast<void**>(&func), detour, reinterpret_cast<void**>(&ret));
        GW::Hook::EnableHooks(func);
        return true;
    }

    bool EnsureMultilineInitialised()
    {
        static bool initialised = false;
        if (!initialised) {
            initialised = true;
            InstallHook(TextMultiline_UICallback_Func, OnTextMultilineUICallback, TextMultiline_UICallback_Ret,
                        GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x83\xc0\xf8\x83\xf8\x5b", "xxxxxx")), "TextMultiline_UICallback");
        }
        return TextMultiline_UICallback_Func != nullptr;
    }
    bool EnsureButtonInitialised()
    {
        static bool initialised = false;
        if (!initialised) {
            initialised = true;
            InstallHook(CtlButton_UICallback_Func, OnCtlButtonUICallback, CtlButton_UICallback_Ret,
                        GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("CtlBtn.cpp", "checked", 0, 0)), "CtlButton_UICallback");
        }
        return CtlButton_UICallback_Func != nullptr;
    }

    template <typename FrameT>
    FrameT* FindByEncodedString(std::unordered_map<std::wstring, FrameT*>& cache, const wchar_t* encoded_string, bool partial_match)
    {
        if (!partial_match) {
            const auto found = cache.find(encoded_string);
            return found == cache.end() ? nullptr : found->second;
        }
        for (const auto& [label, frame] : cache) {
            if (label.starts_with(encoded_string)) {
                return frame;
            }
        }
        return nullptr;
    }
}

namespace GW::UI {
    GW::MultiLineTextLabelFrame* GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        if (!encoded_string || !EnsureMultilineInitialised()) {
            return nullptr;
        }
        return FindByEncodedString(multiline_frames, encoded_string, partial_match);
    }

    GW::ButtonFrame* GetButtonFrameByEncodedString(const wchar_t* encoded_string, const bool partial_match)
    {
        if (!encoded_string || !EnsureButtonInitialised()) {
            return nullptr;
        }
        return FindByEncodedString(button_frames, encoded_string, partial_match);
    }
}
