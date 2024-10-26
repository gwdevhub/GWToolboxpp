#include <imgui.h>
#include <ImGuiCppWrapper.h>

// Copied from https://github.com/ocornut/imgui/tree/master/misc/cpp, which did not compile somehow

namespace ImGui 
{
    struct InputTextCallback_UserData {
        std::string* Str;
        ImGuiInputTextCallback ChainCallback;
        void* ChainCallbackUserData;
    };
    static int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            // Resize string callback
            // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
            std::string* str = user_data->Str;
            IM_ASSERT(data->Buf == str->c_str());
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        else if (user_data->ChainCallback) {
            // Forward to user callback, if any
            data->UserData = user_data->ChainCallbackUserData;
            return user_data->ChainCallback(data);
        }
        return 0;
    }
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return ImGui::InputText(label, const_cast<char*>(str->c_str()), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }
    void ShowHelp(const char* help)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", "(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", help);
        }
    }
} // namespace ImGui
