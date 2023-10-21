#pragma once

#include <ToolboxModule.h>
#include <Utf8.h>

namespace GuiUtils {
    class EncString;
}

namespace GW::Constants {
    enum class Profession;
    enum class MapID;
    enum class SkillID : uint32_t;
}
namespace GW {
    struct Item;
}

class Resources : public ToolboxModule {
    friend class GWToolbox;
    Resources();
    ~Resources() override;

public:
    Resources(const Resources&) = delete;

    static Resources& Instance()
    {
        static Resources instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Resources"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;

    void Update(float delta) override;
    static void DxUpdate(IDirect3DDevice9* device);

    // Enqueue instruction to be called on worker thread, away from the render loop e.g. curl requests
    static void EnqueueWorkerTask(const std::function<void()>& f);
    // Enqueue instruction to be called on the main update loop of GW
    static void EnqueueMainTask(const std::function<void()>& f);
    // Enqueue instruction to be called on the draw loop of GW e.g. messing with DirectX9 device
    static void EnqueueDxTask(const std::function<void(IDirect3DDevice9*)>& f);

    static void OpenFileDialog(std::function<void(const char*)> callback, const char* filterList = nullptr, const char* defaultPath = nullptr);
    static void SaveFileDialog(std::function<void(const char*)> callback, const char* filterList = nullptr, const char* defaultPath = nullptr);

    static int LoadIniFromFile(const std::filesystem::path& absolute_path, ToolboxIni* inifile);
    static int SaveIniToFile(const std::filesystem::path& absolute_path, const ToolboxIni* inifile);

    static std::filesystem::path GetComputerFolderPath();
    static std::filesystem::path GetSettingsFolderPath();

private:
    static bool SetSettingsFolder(const std::filesystem::path& foldername);

public:
    static std::filesystem::path GetSettingFile(const std::filesystem::path& file);
    static std::filesystem::path GetPath(const std::filesystem::path& file);
    static std::filesystem::path GetPath(const std::filesystem::path& folder, const std::filesystem::path& file);
    static HRESULT ResolveShortcut(const std::filesystem::path& in_shortcut_path, std::filesystem::path& out_actual_path);

    static utf8::string GetPathUtf8(const std::wstring& file);
    static bool EnsureFolderExists(const std::filesystem::path& path);

    // Returns current scale multiplier based on gw preferences. Cached for per frame access, pass force = true to get fresh from gw settings.
    static float GetGWScaleMultiplier(bool force = false);

    // Generic callback used when loading async functions. If success is false, any error details are held in response.
    using AsyncLoadCallback = std::function<void(bool success, const std::wstring& response)>;
    // Callback for binary, usually only curl stuff; try to stick to wstrings where possible
    using AsyncLoadMbCallback = std::function<void(bool success, const std::string& response)>;

    // Load from file to D3DTexture, runs callback on completion
    static void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, AsyncLoadCallback callback = nullptr);
    // Load from compiled resource id to D3DTexture, runs callback on completion
    static void LoadTexture(IDirect3DTexture9** texture, WORD id, AsyncLoadCallback callback = nullptr);
    // Load from file to D3DTexture, fallback to resource id, runs callback on completion
    static void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, WORD id, AsyncLoadCallback callback = nullptr);
    // Load from file to D3DTexture, fallback to remote location, runs callback on completion
    static void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback = nullptr);

    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetProfessionIcon(GW::Constants::Profession p);
    // Fetches skill image from gw dat via file_id
    static IDirect3DTexture9** GetSkillImage(GW::Constants::SkillID skill_id);
    // Fetches skill page from GWW, parses out the image for the skill then downloads that to disk
    // Not elegant, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetSkillImageFromGWW(GW::Constants::SkillID skill_id);

    static IDirect3DTexture9** GetItemImage(GW::Item* item);
    // Fetches item page from GWW, parses out the image for the item then downloads that to disk
    // Not elegant, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetItemImage(const std::wstring& item_name);
    // Fetches File page from GWW, parses out the image for the file given
    // Not elegant, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetGuildWarsWikiImage(const char* filename, size_t width = 0);

    // Guaranteed to return a pointer, but may not yet be decoded.
    static GuiUtils::EncString* GetMapName(GW::Constants::MapID map_id);
    // Guaranteed to return a pointer, but may not yet be decoded.
    static GuiUtils::EncString* DecodeStringId(uint32_t enc_str_id);

    // Ensure file exists on disk, download from remote location if not found. If an error occurs, details are held in error string
    static void EnsureFileExists(const std::filesystem::path& path_to_file, const std::string& url, const AsyncLoadCallback& callback);

    // download to file, blocking. If an error occurs, details are held in response string
    static bool Download(const std::filesystem::path& path_to_file, const std::string& url, std::wstring& response);
    // download to file, async, calls callback on completion. If an error occurs, details are held in response string
    void Download(const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback) const;
    // download to memory, blocking. If an error occurs, details are held in response string
    static bool Download(const std::string& url, std::string& response);
    // download to memory, async, calls callback on completion. If an error occurs, details are held in response string
    void Download(const std::string& url, AsyncLoadMbCallback callback) const;


    // download to memory, blocking. If an error occurs, details are held in response string
    static bool Post(const std::string& url, const std::string& payload, std::string& response);
    // download to memory, async, calls callback on completion. If an error occurs, details are held in response string
    static void Post(const std::string& url, const std::string& payload, AsyncLoadMbCallback callback);

    // Stops the worker thread once it's done with the current jobs.
    void EndLoading() const;

private:
    static void Cleanup();
    // Assign IDirect3DTexture9* from file
    static HRESULT TryCreateTexture(IDirect3DDevice9* device, const std::filesystem::path& path_to_file, IDirect3DTexture9** texture, std::wstring& error);
    // Assign IDirect3DTexture9* from resource
    static HRESULT TryCreateTexture(IDirect3DDevice9* pDevice, HMODULE hSrcModule, LPCSTR pSrcResource, IDirect3DTexture9** texture, std::wstring& error);
    // Copy from compiled resource binary to file on local disk.
    static bool ResourceToFile(WORD id, const std::filesystem::path& path_to_file, std::wstring& error);
};
