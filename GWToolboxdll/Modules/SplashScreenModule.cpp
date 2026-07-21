#include "stdafx.h"

#include <DirectXTex.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <Logger.h>
#include <ImGuiAddons.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TextUtils.h>

#include "Resources.h"
#include "SplashScreenModule.h"

namespace {
    // Field offsets into Gw.exe's splash image descriptor:
    // { u32 width, u32 height, u32 compressedSize, void* data, u32 decompressedSize, u32 reserved }.
    constexpr size_t OFF_COMPRESSED_SIZE = 0x08; // patch spans this + the following data pointer
    constexpr size_t OFF_DECOMPRESSED_SIZE = 0x10;

    // One embedded splash image the game can pick (default + zh-TW variants).
    struct SplashImage {
        uintptr_t struct_addr = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t decompressed_size = 0; // == width*height/2 for DXT1
        std::vector<uint8_t> dxt1;      // our replacement blocks; data() must stay stable while patched
        GW::MemoryPatcher patcher;      // rewrites [compressedSize, dataPtr] at struct_addr+8
    };

    // unique_ptr keeps each SplashImage (and its non-movable MemoryPatcher) at a stable address.
    std::vector<std::unique_ptr<SplashImage>> splash_images;
    std::string image_path;

    bool ReadStructField(uintptr_t struct_addr, size_t offset, uint32_t& out)
    {
        const auto addr = struct_addr + offset;
        if (!GW::Scanner::IsValidPtr(addr, GW::ScannerSection::Section_DATA) &&
            !GW::Scanner::IsValidPtr(addr, GW::ScannerSection::Section_RDATA)) {
            return false;
        }
        out = *reinterpret_cast<uint32_t*>(addr);
        return true;
    }

    // Validates a scan hit really is a DXT1 splash descriptor, and records its dimensions.
    bool RegisterSplashStruct(uintptr_t struct_addr)
    {
        if (!struct_addr) {
            return false;
        }
        // The default and zh-TW operands can coincide in some builds; never patch one twice.
        for (const auto& existing : splash_images) {
            if (existing->struct_addr == struct_addr) {
                return false;
            }
        }
        uint32_t width, height, decompressed_size;
        if (!ReadStructField(struct_addr, 0, width) ||
            !ReadStructField(struct_addr, 4, height) ||
            !ReadStructField(struct_addr, OFF_DECOMPRESSED_SIZE, decompressed_size)) {
            return false;
        }
        // DXT1 sanity: sane, 4x4-block-aligned dimensions with 4bpp payload.
        if (width == 0 || height == 0 || width > 4096 || height > 4096 ||
            (width & 3) != 0 || (height & 3) != 0 ||
            decompressed_size != width * height / 2) {
            return false;
        }
        auto img = std::make_unique<SplashImage>();
        img->struct_addr = struct_addr;
        img->width = width;
        img->height = height;
        img->decompressed_size = decompressed_size;
        img->dxt1.resize(decompressed_size); // fixed buffer -> data() is stable for the process lifetime
        // Point the descriptor at our (still empty) buffer and mark it stored-mode (compressed == decompressed
        // makes CmpDecompress a plain memcpy). We only enable the patch once the buffer holds a real image.
        const auto buf_ptr = reinterpret_cast<uint32_t>(img->dxt1.data());
        uint8_t patch[8];
        memcpy(patch + 0, &decompressed_size, 4);
        memcpy(patch + 4, &buf_ptr, 4);
        img->patcher.SetPatch(struct_addr + OFF_COMPRESSED_SIZE, reinterpret_cast<const char*>(patch), sizeof(patch));
        Log::Log("[SplashScreen] found splash art descriptor @ 0x%p (%ux%u)\n", (void*)struct_addr, width, height);
        splash_images.push_back(std::move(img));
        return true;
    }

    // Decodes image_path, resizes to (width,height) and writes exactly `size` bytes of DXT1 into dst.
    bool EncodeDxt1(uint32_t width, uint32_t height, uint8_t* dst, size_t size)
    {
        if (image_path.empty()) {
            return false;
        }
        // WIC (PNG/JPG) needs COM on this thread; the splash runs early so init defensively. Never uninit:
        // it would tear down DirectXTex's cached WIC factory. A stray init ref is harmless in the game process.
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        const std::filesystem::path path = TextUtils::StringToWString(image_path);
        DirectX::ScratchImage loaded;
        HRESULT hr = path.extension() == L".dds"
            ? DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, loaded)
            : DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, loaded);
        if (FAILED(hr)) {
            Log::Log("[SplashScreen] failed to load '%s' (0x%08X)\n", image_path.c_str(), hr);
            return false;
        }

        // Normalise to uncompressed RGBA so Resize/Compress have a plain source.
        const auto& meta = loaded.GetMetadata();
        DirectX::ScratchImage rgba;
        hr = DirectX::IsCompressed(meta.format)
            ? DirectX::Decompress(loaded.GetImages(), loaded.GetImageCount(), meta, DXGI_FORMAT_R8G8B8A8_UNORM, rgba)
            : DirectX::Convert(loaded.GetImages(), loaded.GetImageCount(), meta, DXGI_FORMAT_R8G8B8A8_UNORM,
                               DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, rgba);
        if (FAILED(hr)) {
            return false;
        }

        DirectX::ScratchImage resized;
        if (rgba.GetMetadata().width != width || rgba.GetMetadata().height != height) {
            hr = DirectX::Resize(rgba.GetImages(), rgba.GetImageCount(), rgba.GetMetadata(),
                                 width, height, DirectX::TEX_FILTER_DEFAULT, resized);
            if (FAILED(hr)) {
                return false;
            }
        }
        else {
            resized = std::move(rgba);
        }

        // The game expects 4bpp DXT1 (BC1); force it even with source alpha (BC1 keeps 1-bit alpha).
        DirectX::ScratchImage bc1;
        hr = DirectX::Compress(resized.GetImages(), resized.GetImageCount(), resized.GetMetadata(),
                               DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, bc1);
        if (FAILED(hr)) {
            return false;
        }
        const DirectX::Image* out = bc1.GetImage(0, 0, 0);
        if (!out || out->slicePitch != size) {
            Log::Log("[SplashScreen] encoded size mismatch (%zu != %zu)\n", out ? out->slicePitch : 0u, size);
            return false;
        }
        memcpy(dst, out->pixels, size);
        return true;
    }

    // Re-encodes the configured image into every descriptor's buffer and toggles the patches.
    void RefreshPatch()
    {
        const bool want = !image_path.empty();
        for (const auto& img : splash_images) {
            if (want && EncodeDxt1(img->width, img->height, img->dxt1.data(), img->decompressed_size)) {
                img->patcher.TogglePatch(true);
            }
            else {
                img->patcher.TogglePatch(false);
            }
        }
    }
}

void SplashScreenModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::RegisterField(this, "image_path", &image_path, "Path to the replacement splash image");

    // Locate the language-based splash-art selector (FUN_0083c7e0): the two `mov eax, <struct>`
    // immediates it chooses between are the default and zh-TW image descriptors.
    const uintptr_t selector = GW::Scanner::Find(
        "\x83\xFE\x03\x5E\x75\x0A\x83\xF8\x06\xB8\x00\x00\x00\x00\x74\x05\xB8\x00\x00\x00\x00\xC3",
        "xxxxxxxxxx????xxx????x", 0);
    if (!selector) {
        Log::Log("[SplashScreen] splash-art selector not found; module inactive\n");
        return;
    }
    RegisterSplashStruct(*reinterpret_cast<uintptr_t*>(selector + 17)); // default (English etc.)
    RegisterSplashStruct(*reinterpret_cast<uintptr_t*>(selector + 10)); // zh-TW
}

void SplashScreenModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy); // populates image_path
    RefreshPatch();
}

void SplashScreenModule::Terminate()
{
    for (const auto& img : splash_images) {
        img->patcher.TogglePatch(false); // restore the original splash art
    }
    splash_images.clear();
    ToolboxModule::Terminate();
}

void SplashScreenModule::DrawSettingsInternal()
{
    ImGui::TextWrapped("Replaces the artwork on the Guild Wars startup splash window.");
    ImGui::TextDisabled("Changes take effect the next time you launch Guild Wars, since the splash "
                        "is drawn before Toolbox can redraw it.");
    ImGui::Spacing();

    if (splash_images.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Splash art not found in this Guild Wars build - module inactive.");
        return;
    }

    const uint32_t w = splash_images.front()->width;
    const uint32_t h = splash_images.front()->height;
    ImGui::Text("Native splash size: %ux%u (images are resized to fit)", w, h);

    if (ImGui::InputText("Image", image_path, 1024)) {
        // applied via the Apply button so we don't re-encode on every keystroke
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        Resources::OpenFileDialog([](const char* path) {
            if (!path) {
                return;
            }
            const std::string picked = path;
            Resources::EnqueueMainTask([picked] {
                image_path = picked;
                RefreshPatch();
            });
        }, "png,jpg,jpeg,bmp,dds");
    }

    if (ImGui::Button("Apply")) {
        RefreshPatch();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        image_path.clear();
        RefreshPatch();
    }

    const bool active = !splash_images.empty() && splash_images.front()->patcher.GetIsActive();
    ImGui::SameLine();
    if (active) {
        ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "Replacement ready");
    }
    else if (!image_path.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Could not load image (see log)");
    }
}
