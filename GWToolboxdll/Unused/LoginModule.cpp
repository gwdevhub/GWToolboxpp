#include "stdafx.h"

#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>

#include <Timer.h>

#include "LoginModule.h"
#include <Utils/ToolboxUtils.h>

namespace {
    clock_t state_timestamp = 0;
    uint32_t char_sort_order = std::numeric_limits<uint32_t>::max();

    void SetCharSortOrder(uint32_t order)
    {
        const auto current_sort_order = GetPreference(GW::UI::EnumPreference::CharSortOrder);
        if (char_sort_order == std::numeric_limits<uint32_t>::max()) {
            char_sort_order = current_sort_order;
        }
        if (current_sort_order != order && order != std::numeric_limits<uint32_t>::max()) {
            SetPreference(GW::UI::EnumPreference::CharSortOrder, order);
        }
    }

    enum class LoginState {
        Idle,
        PendingLogin,
        FindCharacterIndex,
        SelectChar
    } state;

    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0;

    bool IsCharSelectReady()
    {
        const GW::PreGameContext* pgc = GW::GetPreGameContext();
        if (!pgc || !pgc->chars.valid()) {
            return false;
        }
        uint32_t ui_state = 10;
        SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
        return ui_state == 2;
    }


#if 0
    const bool enable_login_scene_changing = true;
#else
    const bool enable_login_scene_changing = false;
#endif

    using GetStringParameter_pt = wchar_t*(__cdecl*)(uint32_t param_id_plus_0x27);
    GetStringParameter_pt GetStringParameter_Func = nullptr;
    GetStringParameter_pt GetStringParameter_Ret = nullptr;

    using LoadModelFromDat_pt = void* (__cdecl*)(const wchar_t* filename, float* pos, uint32_t flags, uint32_t param_4);
    LoadModelFromDat_pt LoadModelFromDat_Func = 0, LoadModelFromDat_Ret = 0;
    using CloseHandle_pt = void* (__cdecl*)(void* handle);
    CloseHandle_pt CloseHandle_Func = 0;
    
    using ReadModelDataFromFile_pt = void* (__cdecl*)(const wchar_t* p_filename, int* param_2, void* param_3, void** param_4);
    ReadModelDataFromFile_pt ReadModelDataFromFile_Func = 0, ReadModelDataFromFile_Ret = 0;

    using GetFileId_pt = uint32_t (__cdecl*)(const wchar_t* filename);
    GetFileId_pt GetFileId_Func = 0, GetFileId_Ret = 0;    

    typedef void* (__cdecl* OpenFileAtPath_pt)(const wchar_t* filename, uint32_t flags, uint32_t* errcode);
    OpenFileAtPath_pt OpenFileAtPath_Func = 0, OpenFileAtPath_Ret = 0;

    typedef void* (__cdecl* OpenFileById_pt)(void* hArchive, uint32_t fileId, byte streamId, uint32_t flags, uint32_t* errcode);
    OpenFileById_pt OpenFileById_Func = 0, OpenFileById_Ret = 0;

    typedef uint32_t* (__cdecl* CreateTexture_pt)(const wchar_t* filename, uint32_t flags);
    CreateTexture_pt CreateTexture_Func = 0, CreateTexture_Ret = 0;

    typedef void* (__fastcall* DecompressFile_pt)(void* hArchive, uint32_t fileId, uint32_t bytes);
    DecompressFile_pt DecompressFile_Func = 0, DecompressFile_Ret = 0;

    /*
    * 
    * NEW ARRAY
    0095B410  9CCE 0104 0000 0000 
    9D0F 0104 0000 0000  
0095B420  9C33 0104 0000 0000 
9C32 0104 0000 0000  
0095B430  83EF 0103 0000 0000 
8518 0102 0000 0000  
0095B440  9D0C 0104 0000 0000 
9CD7 0104 0000 0000  
0095B450  9CE1 0104 0000 0000 
0044 006E 0053 0074  


0b 85 02 01 00 00 00 00 
00 84 03 01 00 00 00 00 
15 85 02 01 00 00 00 00 
ef 83 03 01 00 00 00 00 
f0 83 03 01 00 00 00 00 
18 85 02 01 00 00 00 00 
0e 85 02 01 00 00 00 00 
2b 85 02 01 00 00 00 00 
36 84 03 01 00 00 00 00 
0e 85 02 01 00 00 00 00 
b4 7b 01 01 00 00 00 00

        const wchar_t* overrides[] = {
            L"\x850b\x102",
            L"\x8400\x103",
            L"\x8515\x102",
            L"\x83ef\x103",
            L"\x83f0\x103",
            L"\x8518\x102",
            L"\x850e\x102",
            //L"\x852b\x102",
            L"\x8436\x103",
            L"\x850e\x102",
            L"\x7bb4\x101",
        };


    */

    std::vector<void*> extra_recobj_handles_for_login_scene;

    using AssetIndex = std::map<uint32_t, std::filesystem::path>;
    AssetIndex prophecies_asset_files_by_file_id;
    AssetIndex factions_asset_files_by_file_id;
    AssetIndex nightfall_asset_files_by_file_id;
    AssetIndex eotn_asset_files_by_file_id;

    enum class LoginScreen {
        Prophecies,
        Factions,
        Nightfall,
        Eotn
    };

    LoginScreen current_login_screen = LoginScreen::Eotn;
    AssetIndex* asset_index_for_login_screen = &eotn_asset_files_by_file_id;

    // Any extra models or scene adjustments
    void BuildLoginScene() {
        // Any new models created via LoadModelFromDat should be added to extra_recobj_handles_for_login_scene
        // 
        switch (current_login_screen) {
        case LoginScreen::Factions:
            // TODO: Lighting
            // TODO: Reflective sea
            // ...???
            break;
        case LoginScreen::Nightfall:
            // TODO: Lighting
            // TODO: Reflective sea
            // ...???
            break;
        }
    }
    void DestroyLoginScene() {
        for (auto handle : extra_recobj_handles_for_login_scene) {
            CloseHandle_Func(handle);
        }
        extra_recobj_handles_for_login_scene.clear();
    }
    const uint32_t FileHashToFileId(const wchar_t* param_1) {
        if (!param_1)
            return 0;
        if (((0xff < *param_1) && (0xff < param_1[1])) &&
            ((param_1[2] == 0 || ((0xff < param_1[2] && (param_1[3] == 0)))))) {
            return (*param_1 - 0xff00ff) + (uint32_t)param_1[1] * 0xff00;
        }
        return 0;
    }

    void SetLoginScene(LoginScreen campaign) {
        switch (campaign) {
        case LoginScreen::Prophecies:
            asset_index_for_login_screen = &prophecies_asset_files_by_file_id;
            break;
        case LoginScreen::Factions:
            asset_index_for_login_screen = &factions_asset_files_by_file_id;
            break;
        case LoginScreen::Nightfall:
            asset_index_for_login_screen = &nightfall_asset_files_by_file_id;
            break;
        case LoginScreen::Eotn:
            asset_index_for_login_screen = &eotn_asset_files_by_file_id;
            break;
        }
        current_login_screen = campaign;
    }


    bool replace_assets = false;

    void* __fastcall OnDecompressFile(void* hArchive, uint32_t file_id, uint32_t bytes) {
        GW::Hook::EnterHook();
        if (!(replace_assets && GW::GetPreGameContext())) {
            GW::Hook::LeaveHook();
            return DecompressFile_Ret(hArchive, file_id, bytes);
        }
        const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
        if (found != asset_index_for_login_screen->end()) {
            GW::Hook::LeaveHook();
            return DecompressFile_Ret(hArchive, file_id, bytes);
        }
        GW::Hook::LeaveHook();
        return DecompressFile_Ret(hArchive, file_id, bytes);
    }

    uint32_t* OnCreateTexture(const wchar_t* filename, uint32_t flags) {
        GW::Hook::EnterHook();
        if (!(replace_assets && GW::GetPreGameContext())) {
            GW::Hook::LeaveHook();
            return CreateTexture_Ret(filename, flags);
        }
        const auto file_id = FileHashToFileId(filename);
        const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
        if (found != asset_index_for_login_screen->end()) {
            filename = found->second.c_str();
        }
        GW::Hook::LeaveHook();
        return CreateTexture_Ret(filename, flags);
    }

    uint32_t OnGetFileId(const wchar_t* filename) {
        GW::Hook::EnterHook();
        if (!(replace_assets && GW::GetPreGameContext())) {
            GW::Hook::LeaveHook();
            return GetFileId_Ret(filename);
        }
        const auto file_id = FileHashToFileId(filename);
        const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
        if (found != asset_index_for_login_screen->end()) {
            GW::Hook::LeaveHook();
            return 0;
        }
        GW::Hook::LeaveHook();
        return GetFileId_Ret(filename);
    }

    void* OnOpenFilebyId(void* hArchive, uint32_t file_id, byte streamId, uint32_t flags, uint32_t* errcode) {
        GW::Hook::EnterHook();
        if (!(replace_assets && GW::GetPreGameContext())) {
            GW::Hook::LeaveHook();
            return OpenFileById_Ret(hArchive, file_id, streamId, flags,errcode);
        }
        const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
        if (found != asset_index_for_login_screen->end()) {
            GW::Hook::LeaveHook();
            return OpenFileAtPath_Ret(found->second.c_str(), flags, errcode);
        }
        GW::Hook::LeaveHook();
        return OpenFileById_Ret(hArchive, file_id, streamId, flags, errcode);
    }

    void* OnOpenFileAtPath(const wchar_t* filename, uint32_t flags, uint32_t* errcode) {
        GW::Hook::EnterHook();
        if (!(replace_assets && GW::GetPreGameContext())) {
            GW::Hook::LeaveHook();
            return OpenFileAtPath_Ret(filename, flags, errcode);
        }
        auto file_id = FileHashToFileId(filename);
        const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
        if (found != asset_index_for_login_screen->end()) {
            filename = found->second.c_str();
        }
        GW::Hook::LeaveHook();
        return OpenFileAtPath_Ret(filename, flags, errcode);
    }
    
    // Models swapped here!
    void* OnLoadModelFromDat(const wchar_t* filename, float* pos, uint32_t flags, uint32_t param_4) {
        GW::Hook::EnterHook();
        if (!GW::GetPreGameContext()) {
            GW::Hook::LeaveHook();
            return LoadModelFromDat_Ret(filename, pos, flags, param_4);
        }
        switch (current_login_screen) {
        case LoginScreen::Prophecies:
            if (wcscmp(filename, L"\x9cce\x104") == 0) {
                // Main prop
                replace_assets = true;
                filename = L"\x1156\x0102";
                flags = 0x22;
            }
            break;
        case LoginScreen::Factions:
            if (wcscmp(filename, L"\x9cce\x104") == 0) {
                // Main prop
                replace_assets = true;
                filename = L"\x850b\x102";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x9D0C\x0104") == 0) {
                // Skybox?
                //replace_assets = true;
                filename = L"\x852b\x102";
                flags = 0xa2;
            }
            break;
        case LoginScreen::Nightfall:
            if (wcscmp(filename, L"\x9cce\x104") == 0) {
                // Main prop, first model to load
                replace_assets = true;
                filename = L"\x850b\x102";
                //filename = L"\xd1d4\x105";
                flags = 0x22;
            }
            /*else if (wcscmp(filename, L"\x9D0F\x0104") == 0) {
                filename = L"\x8400\x103";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x9C33\x0104") == 0) {
                filename = L"\x8515\x102";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x9C32\x0104") == 0) {
                filename = L"\x83ef\x103";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x83EF\x103") == 0) {
                filename = L"\x83f0\x103";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x8518\x0102") == 0) {
                filename = L"\x8518\x0102";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x9D0C\x0104") == 0) {
                // Skybox?
                filename = L"\x9cf3\x104";
                flags = 0xa2;
            }
            else if (wcscmp(filename, L"\x9CD7\x0104") == 0) {
                filename = L"\x850e\x102";
                flags = 0x22;
            }
            else if (wcscmp(filename, L"\x9Ce1\x0104") == 0) {
                filename = L"\x8436\x103";
                flags = 0x22;
            }*/
            break;
        }
        if (replace_assets) {
            auto file_id = FileHashToFileId(filename);
            const auto found = file_id ? asset_index_for_login_screen->find(file_id) : asset_index_for_login_screen->end();
            if (found != asset_index_for_login_screen->end()) {
                filename = found->second.c_str();
            }
        }
        // NB: Nightfall seems to fail loading a material; not sure which one atm
        const auto res =  LoadModelFromDat_Ret(filename, pos, flags, param_4);
        if (wcscmp(filename, L"\x9Ce1\x0104") == 0) {
            replace_assets = false;
        }
        GW::Hook::LeaveHook();
        return res;
    }

    // Always ensure character name has been pre-filled; this isn't actually used in practice as the security character feature is deprecated.
    // Prefilling it ensures that auto login can work without -charname argument being given.
    wchar_t* original_charname_parameter = nullptr;

    wchar_t* OnGetStringParameter(const uint32_t param_id)
    {
        GW::Hook::EnterHook();
        wchar_t* parameter_value = GetStringParameter_Ret(param_id);
        wchar_t* cmp = nullptr;
        GW::UI::GetCommandLinePref(L"character", &cmp);
        if (cmp == parameter_value) {
            // charname parameter
            original_charname_parameter = parameter_value;
            parameter_value = const_cast<wchar_t*>(L"NA");
        }
        //Log::Info("GetStringParameter_Ret %p = %ls", param_id, parameter_value);
        GW::Hook::LeaveHook();
        return parameter_value;
    }

    using PortalAccountLogin_pt = void(__cdecl*)(uint32_t transaction_id, uint32_t* user_id, uint32_t* session_id, wchar_t* preselect_character);
    PortalAccountLogin_pt PortalAccountLogin_Func = nullptr;
    PortalAccountLogin_pt PortalAccountLogin_Ret = nullptr;

    // Ensure we're asking for a valid character on login if given as a parameter
    void OnPortalAccountLogin(const uint32_t transaction_id, uint32_t* user_id, uint32_t* session_id, wchar_t* preselect_character)
    {
        GW::Hook::EnterHook();
        // Don't pre-select the character yet; we'll do this in the Update() loop after successful login
        preselect_character[0] = 0;
        PortalAccountLogin_Ret(transaction_id, user_id, session_id, preselect_character);
        state_timestamp = TIMER_INIT();
        state = LoginState::PendingLogin;
        GW::Hook::LeaveHook();
    }

    void InitialiationFailure(const char* msg = nullptr)
    {
        Log::Error(msg ? msg : "Failed to initialise LoginModule");
#if _DEBUG
        ASSERT(false);
#endif
    }


    GW::UI::UIInteractionCallback PreGameScene_UICallback_Func = nullptr, PreGameScene_UICallback_Ret = nullptr;
    std::vector<void*> login_scene_handles;

    void OnPreGameScene_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        PreGameScene_UICallback_Ret(message, wParam, lParam);
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) {
            DestroyLoginScene();
        }
        if (message->message_id == GW::UI::UIMessage::kInitFrame) {
            BuildLoginScene();
        }
        GW::Hook::LeaveHook();
    }
}

void LoginModule::Initialize()
{
    ToolboxModule::Initialize();

    state = LoginState::Idle;

    PortalAccountLogin_Func = (PortalAccountLogin_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\xc7\x45\xe8\x38\x00\x00\x00\x89\x4d\xf0", "xxxxxxxxxx"));
    if (!PortalAccountLogin_Func) {
        Log::Warning("The fix to pre-select a character isn't working; portal login process may have changed.\nThis was here to allow reconnect dialog if you're on a diff char, check is this is fixed in vanilla!");
        //return InitialiationFailure("Failed to initialize PortalAccountLogin_Func");
    }

    GetStringParameter_Func = (GetStringParameter_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Param.cpp", "string - PARAM_STRING_FIRST < (sizeof(s_strings) / sizeof((s_strings)[0]))",0,0));
    if (!GetStringParameter_Func) {
        return InitialiationFailure("Failed to initialize GetStringParameter_Func");
    }
    {
        int res = 0;
        if (PortalAccountLogin_Func) {
            res = GW::Hook::CreateHook((void**)&PortalAccountLogin_Func, OnPortalAccountLogin, (void**)&PortalAccountLogin_Ret);
            if (res == -1) {
                return InitialiationFailure("Failed to hook PortalAccountLogin_Func");
            }
        }

        res = GW::Hook::CreateHook((void**)&GetStringParameter_Func, OnGetStringParameter, (void**)&GetStringParameter_Ret);
        if (res == -1) {
            return InitialiationFailure("Failed to hook GetStringParameter_Func");
        }

        GW::Hook::EnableHooks(PortalAccountLogin_Func);
        GW::Hook::EnableHooks(GetStringParameter_Func);
    }

    uintptr_t address = GW::Scanner::FindAssertion("UiPregame.cpp", "!s_scene",0,0);

    PreGameScene_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(address);

    uintptr_t CreateLoginBackgroundScene_Func = GW::Scanner::FunctionFromNearCall(address + 0x90);
    uintptr_t FreeLoginBackgroundScene_Func = GW::Scanner::FunctionFromNearCall(address + 0xa8);

    CloseHandle_Func = (CloseHandle_pt)GW::Scanner::FunctionFromNearCall(FreeLoginBackgroundScene_Func + 0x27);
    LoadModelFromDat_Func = (LoadModelFromDat_pt)GW::Scanner::FunctionFromNearCall(CreateLoginBackgroundScene_Func + 0x91);
    ReadModelDataFromFile_Func = (ReadModelDataFromFile_pt)GW::Scanner::FunctionFromNearCall(((uintptr_t)LoadModelFromDat_Func) + 0x31);

    if (enable_login_scene_changing) {
        address = GW::Scanner::Find("\x75\x17\xff\x75\x0c\x57", "xxxxxx");
        GetFileId_Func = (GetFileId_pt)GW::Scanner::FunctionFromNearCall(address - 0x10);
        OpenFileAtPath_Func = (OpenFileAtPath_pt)GW::Scanner::FunctionFromNearCall(address + 0x6);
        OpenFileById_Func = (OpenFileById_pt)GW::Scanner::FunctionFromNearCall(address + 0x22);
        CreateTexture_Func = (CreateTexture_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("\\Code\\Engine\\Gr\\grtex2d.cpp", "!(flags & GR_TEXTURE_TRANSFER_OWNERSHIP)",0, 0));
        DecompressFile_Func = (DecompressFile_pt)GW::Scanner::Find("\x75\x14\x68\x1d\x09\x00\x00", "xxxxxxx", -0xc);

        GW::Hook::CreateHook((void**)&OpenFileAtPath_Func, OnOpenFileAtPath, (void**)&OpenFileAtPath_Ret);
        GW::Hook::EnableHooks(OpenFileAtPath_Func);

        GW::Hook::CreateHook((void**)&OpenFileById_Func, OnOpenFilebyId, (void**)&OpenFileById_Ret);
        GW::Hook::EnableHooks(OpenFileById_Func);

        GW::Hook::CreateHook((void**)&GetFileId_Func, OnGetFileId, (void**)&GetFileId_Ret);
        //GW::Hook::EnableHooks(GetFileId_Func);

        GW::Hook::CreateHook((void**)&CreateTexture_Func, OnCreateTexture, (void**)&CreateTexture_Ret);
        //GW::Hook::EnableHooks(CreateTexture_Func);

        GW::Hook::CreateHook((void**)&LoadModelFromDat_Func, OnLoadModelFromDat, (void**)&LoadModelFromDat_Ret);
        GW::Hook::EnableHooks(LoadModelFromDat_Func);

        GW::Hook::CreateHook((void**)&DecompressFile_Func, OnDecompressFile, (void**)&DecompressFile_Ret);
        //GW::Hook::EnableHooks(DecompressFile_Func);

        // Build asset map for login scenes

        auto index_available_assets = [](const std::filesystem::path& folder, AssetIndex& index) {
            if (!(std::filesystem::exists(folder) && std::filesystem::is_directory(folder)))
                return;
            std::filesystem::directory_iterator dir(folder);   // points to the beginning of the folder
            std::filesystem::directory_iterator end;
            while (dir != end) {
                const auto& pathname = dir->path();
                const auto& basename = pathname.filename();
                const auto& pathname_ws = basename.wstring();
                auto file_id_start = pathname_ws.find_first_of('_');
                if (file_id_start != std::wstring::npos) {
                    file_id_start++;
                    const auto file_id_str = pathname_ws.substr(file_id_start, pathname_ws.find_first_of('_', file_id_start) - file_id_start);
                    const auto file_id_dec = wcstol(file_id_str.c_str(), NULL, 10);
                    if (file_id_dec)
                        index[file_id_dec] = pathname;
                }
                dir++;
            }
            };

        // Ue GWMB to export all files to a folder, should be in the format "21_8363_3435390177_ATEXDXT3.gwraw" - then replace this with the folder name.
        index_available_assets(L"C:\\Users\\Jon\\Desktop\\Prophecies_dat", prophecies_asset_files_by_file_id);
        index_available_assets(L"C:\\Users\\Jon\\Desktop\\Nightfall_scene_assets", nightfall_asset_files_by_file_id);
        index_available_assets(L"C:\\Users\\Jon\\Desktop\\Factions_scene_assets", factions_asset_files_by_file_id);
        //index_available_assets(L"C:\\Users\\Jon\\Desktop\\Eotn_scene_assets", eotn_asset_files_by_file_id);
        SetLoginScene(LoginScreen::Eotn);
    }
}

bool LoginModule::HasSettings() {
    return enable_login_scene_changing;
}

void LoginModule::DrawSettingsInternal() {
    if (ImGui::Combo("Login scene", (int*)&current_login_screen, "Prophecies\0Factions\0Nightfall\0Eye of the North")) {
        SetLoginScene(current_login_screen);
    }
}

void LoginModule::Terminate()
{

    if(GetStringParameter_Func) GW::Hook::RemoveHook(GetStringParameter_Func);
    if(PortalAccountLogin_Func) GW::Hook::RemoveHook(PortalAccountLogin_Func);
    if(PreGameScene_UICallback_Func) GW::Hook::RemoveHook(PreGameScene_UICallback_Func);
}

void LoginModule::Update(float)
{
    // This loop checks to see if the player has inputted a -charname argument previously.
    // If they have, then we use the user interface to manually navigate to a character
    // This is because the Portal login above would usually do it, but it overrides any reconnect dialogs.
    switch (state) {
        case LoginState::Idle:
            return;
        case LoginState::PendingLogin: {
            // No charname to switch to
            if (TIMER_DIFF(state_timestamp) > 5000) {
                state = LoginState::Idle;
                return;
            }
            if (!GW::LoginMgr::IsCharSelectReady()) {
                return;
            }
            original_charname_parameter && *original_charname_parameter && GW::LoginMgr::SelectCharacterToPlay(original_charname_parameter, false);
            state = LoginState::Idle;
        }
        break;
    }
}
