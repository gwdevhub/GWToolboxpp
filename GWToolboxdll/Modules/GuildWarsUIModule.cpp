#include "stdafx.h"

#include <GWCA/Constants/QuestIDs.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Modules/GuildWarsUIModule.h>
#include <Logger.h>
#include <Timer.h>

namespace {

    uintptr_t QuestLogFrameCallbacks = 0;

    typedef void(__fastcall* ProcessUICallbackEvent_pt)(uintptr_t vtable,void* edx,void *param_3,void* param_4,void* param_5);
    ProcessUICallbackEvent_pt ProcessUICallbackEvent_Func = 0;
    ProcessUICallbackEvent_pt ProcessUICallbackEvent_Ret = 0;

    void __fastcall OnProcessUICallbackEvent(uintptr_t vtable,  void* edx, GW::UI::InteractionMessage* param_3, void* param_4, void* param_5) {
        GW::Hook::EnterHook();
        if (vtable == QuestLogFrameCallbacks) {
            Log::Log("%d", param_3->message_id);
        }
        ProcessUICallbackEvent_Ret(vtable, edx, param_3, param_4, param_5);
        GW::Hook::LeaveHook();
    }

    struct FrameLayout {
        uint32_t unk;
        uint32_t unk2;
        GW::Array<uint32_t>* unk3;
    };
    struct Frame {
        void* vtable;
        uint32_t frame_id;
        FrameLayout* frame_layout;
    };

    std::map<uint32_t, GW::UI::UIInteractionCallback> toolbox_frame_callbacks;
    
    typedef uint32_t(__cdecl* CreateUIComponent_pt)(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, const wchar_t* name_enc, const wchar_t* component_label);
    CreateUIComponent_pt CreateUIComponent = 0;

    typedef uint32_t(__cdecl* GetChildFrameId_pt)(uint32_t parent_frame_id, uint32_t child_frame_index);
    GetChildFrameId_pt GetChildFrameId = 0;

    typedef void(__fastcall* AddElementToArray_pt)(GW::Array<uint32_t>* arr, uint32_t edx,uint32_t* element);
    AddElementToArray_pt AddElementToArray = 0;

    typedef void(__cdecl* SendFrameMessage_pt)(uint32_t frame_id,uint32_t msg_id,void* wparam,void* lparam);
    SendFrameMessage_pt SendFrameMessage = 0;

    // __thiscall AddRowToFrame(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3)
    typedef void(__fastcall* AddRowToFrame_pt)(FrameLayout* frame,void* edx, uint32_t child_frame_id,uint32_t param_2,uint32_t param_3);
    AddRowToFrame_pt AddRowToFrame = 0;
    // SetFrameRowStyle(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

    typedef void(__cdecl* SetFrameRowStyle_pt)(uint32_t frame_id, uint32_t param_2, uint32_t param_3, uint32_t param_4);
    SetFrameRowStyle_pt SetFrameRowStyle = 0;

    uint32_t* frame_buffer = 0;

    uint32_t GetFrameById(uint32_t frame_id) {
        return frame_buffer[frame_id];
    }

    void OnComponentUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam, GW::UI::UIInteractionCallback return_func) {
        GW::Hook::EnterHook();
        /*switch (message->message_id) {
        case 0x9: // Create
        case 0x2e:// Click ???
        case 0xb: // Destroy
        //    break;
        default: // Ignore all other events
            return_func(message, wparam, lparam);
            GW::Hook::LeaveHook();
            return;
        }*/

        const auto found = toolbox_frame_callbacks.find(message->action_type);
        if (found != toolbox_frame_callbacks.end()) {
            found->second(message,wparam,lparam);
            if (message->message_id == 0xb) {
                toolbox_frame_callbacks.erase(found);
            }
        }
        return_func(message, wparam, lparam);
        GW::Hook::LeaveHook();
    }

    // NB: This is a hook because it could cause a crash if we closed toolbox (and freed the callback address) with the UI component still open
    GW::UI::UIInteractionCallback OnButtonComponentUICallback_Func = 0;
    GW::UI::UIInteractionCallback OnButtonComponentUICallback_Ret = 0;
    void OnButtonComponentUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        OnComponentUICallback(message, wparam, lparam, OnButtonComponentUICallback_Ret);
    }

    GW::UI::UIInteractionCallback OnDropdownListComponentUICallback_Func = 0;
    GW::UI::UIInteractionCallback OnDropdownListComponentUICallback_Ret = 0;
    void OnDropdownListComponentUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        OnComponentUICallback(message, wparam, lparam, OnDropdownListComponentUICallback_Ret);
    }

    GW::UI::UIInteractionCallback OnTextLabelComponentUICallback_Func = 0;
    GW::UI::UIInteractionCallback OnTextLabelComponentUICallback_Ret = 0;
    void OnTextLabelComponentUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        OnComponentUICallback(message, wparam, lparam, OnTextLabelComponentUICallback_Ret);
    }

    uint32_t GetAvailableChildIndex(uint32_t frame_id) {
        for (int i = 0xfe; i != 0; i--) {
            auto found = GetChildFrameId(frame_id, i);
            if (!found)
                return i;
        }
        return 0;
    }

    wchar_t* EncodeString(const wchar_t* label) {
        size_t enc_label_len = wcslen(label) + 4;
        wchar_t* enc_label = (wchar_t*)malloc(enc_label_len * sizeof(wchar_t));
        ASSERT(enc_label && swprintf(enc_label, enc_label_len, L"\x108\x107%s\x1",label) != -1);
        return enc_label;
    }


    uint32_t my_new_button_id = 0;
    void OnGeneralSettingsButtonClicked(GW::UI::InteractionMessage* message, void*, void*) {
        if (message->action_type == my_new_button_id) {
            Log::Log("My New Button interaction %02x", message->message_id);
            if (message->message_id == 0xb)
                my_new_button_id = 0;
            return;
        }
    }

    typedef void(__fastcall* OnDrawGraphicsSettingsLayout_pt)(Frame* frame, void* edx, FrameLayout* wparam);
    OnDrawGraphicsSettingsLayout_pt OnDrawGraphicsSettingsLayout_Func = 0;
    OnDrawGraphicsSettingsLayout_pt OnDrawGraphicsSettingsLayout_Ret = 0;
    void __fastcall OnDrawGraphicsSettingsLayout(Frame* frame, void* edx, FrameLayout* wparam) {
        GW::Hook::EnterHook();
        
        OnDrawGraphicsSettingsLayout_Ret(frame, edx, wparam);

        my_new_button_id = GuildWarsUIModule::CreateCheckbox(frame->frame_id, L"My New Button!", 0x8000, OnGeneralSettingsButtonClicked);

        //AddRowToFrame(param_1,0x40400000,0,0);
        //child_arr.m_size--;
        //AddRowToFrame((void*)frame_layout,0,0x40400000,0,0);

        //AddElementToArray

        

        GW::Hook::LeaveHook();
    }



}

void GuildWarsUIModule::Initialize() {
    ToolboxModule::Initialize();

    uintptr_t address = GW::Scanner::Find("\x33\xd2\x89\x45\x08\xb9\xac\x01\x00\x00", "xxxxxxxxxx", -0x27);
    if (GW::Scanner::IsValidPtr(address, GW::Scanner::Section::TEXT))
        CreateUIComponent = (CreateUIComponent_pt)address;

    address = GW::Scanner::Find("\xff\x75\x0c\x05\x20\x01\x00\x00", "xxxxxxxx", -0x1a);
    if (GW::Scanner::IsValidPtr(address, GW::Scanner::Section::TEXT))
        GetChildFrameId = (GetChildFrameId_pt)address;

    address = GW::Scanner::Find("\x6a\x00\x6a\x00\x6a\x4a\x57", "xxxxxxx", 0x7);
    SendFrameMessage = (SendFrameMessage_pt)GW::Scanner::FunctionFromNearCall(address);

    address = GW::Scanner::Find("\x6a\x03\x68\x00\x40\x22\x00\x56", "xxxxxxxx", -0x4);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address, GW::Scanner::TEXT)) {
        OnButtonComponentUICallback_Func = *(GW::UI::UIInteractionCallback*)address;
        GW::HookBase::CreateHook(OnButtonComponentUICallback_Func, OnButtonComponentUICallback, (void**)&OnButtonComponentUICallback_Ret);
        GW::HookBase::EnableHooks(OnButtonComponentUICallback_Func);
    }

    address = GW::Scanner::Find("\x6a\x09\x68\x00\x13\x00\x00", "xxxxxxx", -0x4);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address, GW::Scanner::TEXT)) {
        OnDropdownListComponentUICallback_Func = *(GW::UI::UIInteractionCallback*)address;
        GW::HookBase::CreateHook(OnDropdownListComponentUICallback_Func, OnDropdownListComponentUICallback, (void**)&OnDropdownListComponentUICallback_Ret);
        GW::HookBase::EnableHooks(OnDropdownListComponentUICallback_Func);
    }

    address = GW::Scanner::Find("\x6a\x02\x68\x00\x80\x02\x00\x56", "xxxxxxxx", -0x4);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address, GW::Scanner::TEXT)) {
        OnTextLabelComponentUICallback_Func = *(GW::UI::UIInteractionCallback*)address;
        GW::HookBase::CreateHook(OnTextLabelComponentUICallback_Func, OnTextLabelComponentUICallback, (void**)&OnTextLabelComponentUICallback_Ret);
        GW::HookBase::EnableHooks(OnTextLabelComponentUICallback_Func);
    }

    address = GW::Scanner::Find("\x68\x2e\x97\x00\x00", "xxxxx", -0x13);
    //address = GW::Scanner::Find("\x8b\xf9\x68\x45\x05\x00\x00", "xxxxxxx", -0x8);
    if (GW::Scanner::IsValidPtr(address, GW::Scanner::Section::TEXT)) {
        OnDrawGraphicsSettingsLayout_Func = (OnDrawGraphicsSettingsLayout_pt)address;
        GW::HookBase::CreateHook(OnDrawGraphicsSettingsLayout_Func, OnDrawGraphicsSettingsLayout, (void**)&OnDrawGraphicsSettingsLayout_Ret);
        GW::HookBase::EnableHooks(OnDrawGraphicsSettingsLayout_Func);
    }

    address = GW::Scanner::Find("\x6a\x01\x6a\x00\x6a\x00\x6a\x08\x8b\xce","xxxxxxxxxx");
    AddRowToFrame = (AddRowToFrame_pt)GW::Scanner::FunctionFromNearCall(address - 0x5);
    SetFrameRowStyle = (SetFrameRowStyle_pt)GW::Scanner::FunctionFromNearCall(address + 0xA);
    //b'\x83\x3c\xb0\x00\x75\x11\x6a\x59'
    address = GW::Scanner::Find("\x83\x3c\xb0\x00\x75\x11\x6a\x59","xxxxxxxx",-0x4);
    frame_buffer = *(uint32_t**)address;

    // b'\x8b\xf9\x68\x45\x05\x00\x00'

    address = GW::Scanner::FindAssertion("p:\\code\\engine\\controls\\ctllayout.cpp", "m_nodes.Count()",0x1d);
    AddElementToArray = (AddElementToArray_pt)GW::Scanner::FunctionFromNearCall(address);

    QuestLogFrameCallbacks = GW::Scanner::Find("\x8b\x4e\x08\x89\x01\x8b\x7e\x08\x85\xff\x75\x14\x6a\x6d", "xxxxxxxxxxxxxx", -0x4);

    ProcessUICallbackEvent_Func = (ProcessUICallbackEvent_pt)GW::Scanner::FunctionFromNearCall(QuestLogFrameCallbacks + 0x5c);

    if (ProcessUICallbackEvent_Func) {
        GW::HookBase::CreateHook(ProcessUICallbackEvent_Func, OnProcessUICallbackEvent, (void**)&ProcessUICallbackEvent_Ret);
        GW::HookBase::EnableHooks(ProcessUICallbackEvent_Func);
    }

    ASSERT(AddElementToArray);


}
void GuildWarsUIModule::Terminate() {
    ToolboxModule::Terminate();

    GW::HookBase::RemoveHook(OnDrawGraphicsSettingsLayout_Func);

    GW::HookBase::RemoveHook(OnButtonComponentUICallback_Func);
    GW::HookBase::RemoveHook(OnDropdownListComponentUICallback_Func);
    GW::HookBase::RemoveHook(OnTextLabelComponentUICallback_Func);
    GW::HookBase::RemoveHook(ProcessUICallbackEvent_Func);
}
uint32_t GuildWarsUIModule::CreateButton(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags, GW::UI::UIInteractionCallback event_callback) {
    uint32_t child_index = GetAvailableChildIndex(parent_frame_id);
    ASSERT(child_index);
    wchar_t* enc_label = EncodeString(label);
    uint32_t frame_id = CreateUIComponent(parent_frame_id,flags,child_index,OnButtonComponentUICallback_Func,enc_label,0);
    // SetSubclassFrameProc_Func(created, OnButtonComponentUICallback, 1, 0);
    free(enc_label);
    if(event_callback) toolbox_frame_callbacks[frame_id] = event_callback;
    return frame_id;
}
uint32_t GuildWarsUIModule::CreateCheckbox(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags, GW::UI::UIInteractionCallback event_callback) {
    uint32_t frame_id = CreateButton(parent_frame_id, label, flags | 0x8000, event_callback);
    SetCheckboxState(frame_id, false);
    return frame_id;
}
uint32_t GuildWarsUIModule::CreateDropdown(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags, GW::UI::UIInteractionCallback event_callback) {
    uint32_t child_index = GetAvailableChildIndex(parent_frame_id);
    ASSERT(child_index);
    wchar_t* enc_label = EncodeString(label);
    uint32_t frame_id = CreateUIComponent(parent_frame_id,flags,child_index,OnDropdownListComponentUICallback_Func,enc_label,0);
    free(enc_label);
    if(event_callback) toolbox_frame_callbacks[frame_id] = event_callback;
    return frame_id;
}
uint32_t GuildWarsUIModule::CreateTextLabel(uint32_t parent_frame_id,const wchar_t* label, uint32_t flags, GW::UI::UIInteractionCallback event_callback) {
    uint32_t child_index = GetAvailableChildIndex(parent_frame_id);
    ASSERT(child_index);
    wchar_t* enc_label = EncodeString(label);
    uint32_t frame_id = CreateUIComponent(parent_frame_id,flags,child_index,OnTextLabelComponentUICallback_Func,enc_label,label);
    free(enc_label);
    if(event_callback) toolbox_frame_callbacks[frame_id] = event_callback;
    return frame_id;
}
void GuildWarsUIModule::AddDropdownListOption(uint32_t frame_id,const wchar_t* label) {
    wchar_t* enc_label = EncodeString(label);
    SendFrameMessage(frame_id,0x46,enc_label,0);
    free(enc_label);
}
void GuildWarsUIModule::SetDropdownListOption(uint32_t frame_id, uint32_t option_idx) {
    // @Robustness: Check the frame to make sure this is a valid option
    SendFrameMessage(frame_id, 0x51, (void*)option_idx,0);
}

void GuildWarsUIModule::SetCheckboxState(uint32_t frame_id, bool checked) {
    SendFrameMessage(frame_id, 0x47, (void*)checked,0);
}


