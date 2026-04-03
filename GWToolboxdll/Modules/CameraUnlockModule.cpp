#include "stdafx.h"

#include <GWCA/GameEntities/Camera.h>

#include <GWCA/Managers/CameraMgr.h>

#include <GWToolbox.h>

#include "CameraUnlockModule.h"
#include <Defines.h>
#include <Keys.h>
#include <Utils/TextUtils.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

namespace {
    const float default_cam_speed = 1000.f;            // 600 units per sec
    const float rotation_speed = DirectX::XM_PI / 3.f; // 6 seconds for full rotation

    bool forward_fix_z = true;
    float cam_speed = default_cam_speed;
    float cam_max_distance = 900.f;

    GW::HookEntry ChatCmdHookEntry;
    std::vector<GW::Camera*> last_camera_by_mode(10);

    typedef void(__cdecl* OnSetCameraMode_pt)(uint32_t camera_mode, bool enabled);
    OnSetCameraMode_pt OnSetCameraMode_Func = nullptr, OnSetCameraMode_Ret = nullptr;

    void OnSetCameraMode(uint32_t camera_mode, bool enabled) {
        GW::Hook::EnterHook();
        const auto cam = GW::CameraMgr::GetCamera();
        if (!cam) {
            OnSetCameraMode_Ret(camera_mode, enabled);
            return GW::Hook::LeaveHook();
        }
        uint32_t old_mode = cam->camera_mode;
        if (cam->h00D8 && cam->camera_mode != camera_mode) {
            auto cpy = new GW::Camera();
            memcpy(cpy, cam, sizeof(*cam));
            if (last_camera_by_mode[cam->camera_mode]) {
                delete last_camera_by_mode[cam->camera_mode];
            }
            last_camera_by_mode[cam->camera_mode] = cpy;
            
        }
        OnSetCameraMode_Ret(camera_mode, enabled);
        if (cam->camera_mode != old_mode && last_camera_by_mode[cam->camera_mode] && cam->camera_mode == 0) {
            // TODO: How to set the camera position to what it was before we died?

            // We can't just do it here because there is further logic in the chain that resets the camera position now that we're alive

        }
        GW::Hook::LeaveHook();
    }

    bool ForwardMovement(float amount, bool true_forward)
    {
        const auto cam = GW::CameraMgr::GetCamera();
        if (!(cam && amount != .0f)) return false;
        if (true_forward) {
            float pitchX = sqrtf(1.f - cam->pitch * cam->pitch);
            cam->look_at_target.x += amount * pitchX * cosf(cam->yaw);
            cam->look_at_target.y += amount * pitchX * sinf(cam->yaw);
            cam->look_at_target.z += amount * cam->pitch;
        }
        else {
            cam->look_at_target.x += amount * cosf(cam->yaw);
            cam->look_at_target.y += amount * sinf(cam->yaw);
        }
        return true;
    }

    bool VerticalMovement(float amount)
    {
        const auto cam = GW::CameraMgr::GetCamera();
        return cam ? cam->look_at_target.z += amount, true : false;
    }

    bool SideMovement(float amount)
    {
        const auto cam = GW::CameraMgr::GetCamera();
        if (!(cam && amount != .0f)) return false;
        cam->look_at_target.x += amount * -sinf(cam->yaw);
        cam->look_at_target.y += amount * cosf(cam->yaw);
        return true;
    }

    bool RotateMovement(float angle)
    {
        if (angle == 0.f) return false;
        // rotation with fixed z (vertical axe)
        const auto cam = GW::CameraMgr::GetCamera();
        if (!cam) return false;
        float pos_x = cam->position.x;
        float pos_y = cam->position.y;
        float px = cam->look_at_target.x - pos_x;
        float py = cam->look_at_target.y - pos_y;

        GW::Vec3f newPos;
        newPos.x = pos_x + (cosf(angle) * px - sinf(angle) * py);
        newPos.y = pos_y + (sinf(angle) * px + cosf(angle) * py);
        newPos.z = cam->look_at_target.z;

        cam->SetYaw(cam->yaw + angle);
        cam->look_at_target = newPos;
        return true;
    }
    void CHAT_CMD_FUNC(CmdCamera)
    {
        if (argc == 1) {
            GW::CameraMgr::UnlockCam(false);
        }
        else {
            const std::wstring arg1 = TextUtils::ToLower(argv[1]);
            if (arg1 == L"lock") {
                GW::CameraMgr::UnlockCam(false);
            }
            else if (arg1 == L"unlock") {
                GW::CameraMgr::UnlockCam(true);
                Log::Flash("Use Q/E, A/D, W/S, X/Z, R and arrows for camera movement");
            }
            else if (arg1 == L"fog") {
                if (argc == 3) {
                    const std::wstring arg2 = TextUtils::ToLower(argv[2]);
                    if (arg2 == L"on") {
                        GW::CameraMgr::SetFog(true);
                    }
                    else if (arg2 == L"off") {
                        GW::CameraMgr::SetFog(false);
                    }
                }
            }
            else if (arg1 == L"speed") {
                if (argc > 2) {
                    const std::wstring arg2 = TextUtils::ToLower(argv[2]);
                    if (arg2 == L"default") {
                        cam_speed = default_cam_speed;
                    }
                    float speed = 0.0f;
                    if (TextUtils::ParseFloat(arg2.c_str(), &speed)) {
                        cam_speed = speed;
                    }
                }
                Log::Flash("Camera speed is now %f", cam_speed);
            }
            else if (arg1 == L"distance") {
                if (argc > 2) {
                    float dist = 900.f;
                    if (TextUtils::ParseFloat(TextUtils::ToLower(argv[2]).c_str(), &dist)) {
                        GW::CameraMgr::SetMaxDist(dist);
                        cam_max_distance = dist;
                    }
                }
            }
            else {
                Log::Error("Invalid argument.");
            }
            
        }
    }
}

void CameraUnlockModule::Terminate()
{
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(&ChatCmdHookEntry);
    GW::CameraMgr::UnlockCam(false);
    if (OnSetCameraMode_Func) {
        GW::Hook::RemoveHook(OnSetCameraMode_Func);
    }
}

void CameraUnlockModule::Initialize()
{
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(&ChatCmdHookEntry, L"cam", CmdCamera);
    GW::Chat::CreateCommand(&ChatCmdHookEntry, L"camera", CmdCamera);

    OnSetCameraMode_Func = (OnSetCameraMode_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindNthUseOfString("mode < arrsize(s_mode)", 1));
    if (OnSetCameraMode_Func) {
        GW::Hook::CreateHook((void**)&OnSetCameraMode_Func, OnSetCameraMode, (void**)&OnSetCameraMode_Ret);
        GW::Hook::EnableHooks(OnSetCameraMode_Func);
    }
    DEBUG_ASSERT(OnSetCameraMode_Func);
}
void CameraUnlockModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(forward_fix_z);
    LOAD_FLOAT(cam_speed);
    LOAD_FLOAT(cam_max_distance);
    cam_max_distance = std::clamp(cam_max_distance, 25.f, 5000.f);
    GW::CameraMgr::SetMaxDist(cam_max_distance);
}

void CameraUnlockModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(forward_fix_z);
    SAVE_FLOAT(cam_speed);
    SAVE_FLOAT(cam_max_distance);
}
void CameraUnlockModule::DrawSettingsInternal()
{
    ToolboxModule::DrawSettingsInternal();
    ImGui::Text("'/cam unlock' options");
    ImGui::Indent();
    ImGui::Checkbox("Fix height when moving forward", &forward_fix_z);
    ImGui::InputFloat("Camera speed", &cam_speed);
    ImGui::Unindent();
    if (ImGui::InputFloat("Camera max distance", &cam_max_distance, 100.f, 100.f, "%.f")) {
        cam_max_distance = std::clamp(cam_max_distance, 25.f, 5000.f);
        GW::CameraMgr::SetMaxDist(cam_max_distance);
    }  
}

bool CameraUnlockModule::WndProc(const UINT Message, const WPARAM wParam, const LPARAM) {
    if (!GW::CameraMgr::GetCameraUnlock()) {
        return false;
    }
    if (GW::Chat::GetIsTyping()) {
        return false;
    }
    if (ImGui::GetIO().WantTextInput) {
        return false;
    }

    switch (Message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            switch (wParam) {
                case VK_A:
                case VK_D:
                case VK_E:
                case VK_Q:
                case VK_R:
                case VK_S:
                case VK_W:
                case VK_X:
                case VK_Z:

                case VK_ESCAPE:
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                    return true;
            }
    }
    return false;

}

void CameraUnlockModule::Update(float delta) {
    if (delta == 0.f) {
        return;
    }
    if (GW::CameraMgr::GetCameraUnlock() && !GW::Chat::GetIsTyping() && !ImGui::GetIO().WantTextInput) {
        static bool keep_forward;

        float forward = 0;
        float vertical = 0;
        float rotate = 0;
        float side = 0;
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow) || keep_forward) {
            forward += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
            forward -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            side += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E)) {
            side -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Z)) {
            vertical -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_X)) {
            vertical += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
            rotate += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
            rotate -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_R)) {
            keep_forward = true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
            keep_forward = false;
        }

        if (GWToolbox::Instance().right_mouse_down && rotate != 0.f) {
            side = rotate;
            rotate = 0.f;
        }

        ForwardMovement(forward * delta * cam_speed, !forward_fix_z);
        VerticalMovement(vertical * delta * cam_speed);
        RotateMovement(rotate * delta * rotation_speed);
        SideMovement(side * delta * cam_speed);
        GW::CameraMgr::UpdateCameraPos();
    }
}
