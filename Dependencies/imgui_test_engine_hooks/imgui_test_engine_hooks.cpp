#include "imgui_test_engine_hooks.h"

ImGuiTestEngineHookCallbacks imgui_test_engine_hook_callbacks;

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, const ImGuiID id, const ImRect& bb, const ImGuiLastItemData* item_data)
{
    if (imgui_test_engine_hook_callbacks.item_add) {
        imgui_test_engine_hook_callbacks.item_add(ctx, id, bb, item_data);
    }
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, const ImGuiID id, const char* label, const ImGuiItemStatusFlags flags)
{
    if (imgui_test_engine_hook_callbacks.item_info) {
        imgui_test_engine_hook_callbacks.item_info(ctx, id, label, flags);
    }
}

void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}

const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID)
{
    return nullptr;
}
