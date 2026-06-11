#pragma once

#include <imgui.h>
#include <imgui_internal.h>

// The ImGuiTestEngineHook_* symbols required by IMGUI_ENABLE_TEST_ENGINE are defined in the imgui
// target (one TU per binary) and forward to these runtime-settable callbacks; they only fire while
// ImGuiContext::TestEngineHookItems is true.
struct ImGuiTestEngineHookCallbacks {
    void (*item_add)(ImGuiContext* ctx, ImGuiID id, const ImRect& bb, const ImGuiLastItemData* item_data) = nullptr;
    void (*item_info)(ImGuiContext* ctx, ImGuiID id, const char* label, ImGuiItemStatusFlags flags) = nullptr;
};

extern ImGuiTestEngineHookCallbacks imgui_test_engine_hook_callbacks;
