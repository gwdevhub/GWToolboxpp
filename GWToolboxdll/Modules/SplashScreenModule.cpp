#include "stdafx.h"

#include <fstream>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Logger.h>
#include <ImGuiAddons.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TextUtils.h>

#include "Resources.h"
#include "SplashScreenModule.h"

namespace {
    constexpr uintptr_t MAX_IMAGE_BYTES = 64u * 1024 * 1024;

    // DnCtl image-control ctor (asserts "m_image" @ DnCtl.cpp); __thiscall modelled as __fastcall.
    // Only called for the two splash images, so `flags` picks the layer (see background/foreground).
    typedef void*(__fastcall* DnCtlImageCtor_pt)(void* thisptr, void* edx, uint32_t flags, int* rect,
                                                 uint32_t size, const void* data);
    DnCtlImageCtor_pt DnCtlImageCtor_Func = nullptr;
    DnCtlImageCtor_pt DnCtlImageCtor_Ret = nullptr;

    // A replaceable splash image; `bytes` is the user's file, handed to the ctor verbatim.
    struct SplashLayer {
        const char* name;
        const char* label;
        uint32_t flags; // ctor discriminator
        std::string path;
        std::vector<uint8_t> bytes;
    };

    SplashLayer background{"background", "Background image", 0};
    SplashLayer foreground{"foreground", "Foreground logo", 1};
    SplashLayer* layers[] = {&background, &foreground};

    SplashLayer* LayerForFlags(uint32_t flags)
    {
        for (SplashLayer* layer : layers) {
            if (layer->flags == flags) {
                return layer;
            }
        }
        return nullptr;
    }

    void* __fastcall OnDnCtlImageCtor(void* thisptr, void* edx, uint32_t flags, int* rect,
                                      uint32_t size, const void* data)
    {
        GW::Hook::EnterHook();
        if (SplashLayer* layer = LayerForFlags(flags); layer && !layer->bytes.empty()) {
            data = layer->bytes.data();
            size = static_cast<uint32_t>(layer->bytes.size());
        }
        const auto res = DnCtlImageCtor_Ret(thisptr, edx, flags, rect, size, data);
        GW::Hook::LeaveHook();
        return res;
    }

    bool LoadFileBytes(const std::string& path, std::vector<uint8_t>& out)
    {
        if (path.empty()) {
            return false;
        }
        const std::filesystem::path p = TextUtils::StringToWString(path);
        std::error_code ec;
        const auto size = std::filesystem::file_size(p, ec);
        if (ec || size == 0 || size > MAX_IMAGE_BYTES) {
            return false;
        }
        std::ifstream f(p, std::ios::binary);
        if (!f) {
            return false;
        }
        out.resize(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
        return static_cast<uintptr_t>(f.gcount()) == size;
    }

    void RefreshLayer(SplashLayer& layer)
    {
        layer.bytes.clear();
        if (layer.path.empty()) {
            return;
        }
        if (!LoadFileBytes(layer.path, layer.bytes)) {
            layer.bytes.clear();
            Log::Log("[SplashScreen] failed to read %s image '%s'\n", layer.name, layer.path.c_str());
        }
    }

    void RefreshImages()
    {
        for (SplashLayer* layer : layers) {
            RefreshLayer(*layer);
        }
    }

    void DrawLayer(SplashLayer& layer)
    {
        ImGui::PushID(layer.name);
        ImGui::Text("%s", layer.label);
        ImGui::InputText("##path", layer.path, 1024);
        ImGui::SameLine();
        if (ImGui::Button("Browse")) {
            Resources::OpenFileDialog([&layer](const char* path) {
                if (!path) {
                    return;
                }
                const std::string picked = path;
                Resources::EnqueueMainTask([&layer, picked] {
                    layer.path = picked;
                    RefreshLayer(layer);
                });
            }, "png,jpg,jpeg,bmp");
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            layer.path.clear();
            layer.bytes.clear();
        }
        if (!layer.bytes.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "Replacement ready");
        }
        else if (!layer.path.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Could not load image (see log)");
        }
        ImGui::PopID();
    }
}

void SplashScreenModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::RegisterField(this, "background_image", &background.path, "Path to the replacement splash background");
    SettingsRegistry::RegisterField(this, "foreground_image", &foreground.path, "Path to the replacement splash logo");

    DnCtlImageCtor_Func = reinterpret_cast<DnCtlImageCtor_pt>(
        GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("DnCtl.cpp", "m_image", 0, 0), 0xfff));
    if (!DnCtlImageCtor_Func) {
        Log::Log("[SplashScreen] splash image ctor not found; module inactive\n");
        return;
    }
    GW::Hook::CreateHook(reinterpret_cast<void**>(&DnCtlImageCtor_Func), OnDnCtlImageCtor,
                         reinterpret_cast<void**>(&DnCtlImageCtor_Ret));
    GW::Hook::EnableHooks(DnCtlImageCtor_Func);
    Log::Log("[SplashScreen] hooked splash image ctor @ 0x%p\n", (void*)DnCtlImageCtor_Func);
}

void SplashScreenModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy); // populates the image paths
    RefreshImages();
}

void SplashScreenModule::Terminate()
{
    if (DnCtlImageCtor_Func) {
        GW::Hook::DisableHooks(DnCtlImageCtor_Func);
        GW::Hook::RemoveHook(DnCtlImageCtor_Func);
        DnCtlImageCtor_Func = nullptr;
    }
    for (SplashLayer* layer : layers) {
        layer->bytes.clear();
    }
    ToolboxModule::Terminate();
}

void SplashScreenModule::DrawSettingsInternal()
{
    ImGui::TextWrapped("Replaces the background and logo artwork on the Guild Wars startup splash window.");
    ImGui::TextDisabled("Changes take effect the next time you launch Guild Wars, since the splash "
                        "is drawn before Toolbox can redraw it.");
    ImGui::Spacing();

    if (!DnCtlImageCtor_Func) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Splash image constructor not found in this Guild Wars build - module inactive.");
        return;
    }

    DrawLayer(background);
    ImGui::Spacing();
    DrawLayer(foreground);
}
