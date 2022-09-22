#pragma once

#include <RestClient.h>

#include <resource.h>
#include <ToolboxModule.h>
#include <Utf8.h>

namespace GuiUtils {
    class EncString;
}
namespace GW {
    namespace Constants {
        enum class Profession;
        enum class MapID;
        enum class SkillID;
    }
}
class Resources : public ToolboxModule {
    Resources();
    Resources(const Resources&) = delete;
    ~Resources();

    std::recursive_mutex worker_mutex;
    std::recursive_mutex main_mutex;
    std::recursive_mutex dx_mutex;
    void InitRestClient(RestClient* r);
public:
    static Resources& Instance() {
        static Resources instance;
        return instance;
    }

    const char* Name() const override { return "Resources"; }

    void Initialize() override;
    void Terminate() override;

    void Update(float delta) override;
    void DxUpdate(IDirect3DDevice9* device);
    void WorkerUpdate();

    static std::filesystem::path GetSettingsFolderPath();
    static std::filesystem::path GetPath(const std::filesystem::path& file);
    static std::filesystem::path GetPath(const std::filesystem::path& folder, const std::filesystem::path& file);
    static HRESULT ResolveShortcut(const std::filesystem::path& in_shortcut_path, std::filesystem::path& out_actual_path);

    static utf8::string GetPathUtf8(const std::wstring& file);
    static bool EnsureFolderExists(const std::filesystem::path& path);

    // Generic callback used when loading async functions. If success is false, any error details are held in response.
    typedef std::function<void(bool success, const std::wstring& response)> AsyncLoadCallback;
    // Callback for binary, usually only curl stuff; try to stick to wstrings where possible
    typedef std::function<void(bool success, const std::string& response)> AsyncLoadMbCallback;

    // Load from file to D3DTexture, runs callback on completion
    void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, AsyncLoadCallback callback = 0);
    // Load from compiled resource id to D3DTexture, runs callback on completion
    void LoadTexture(IDirect3DTexture9** texture, WORD id, AsyncLoadCallback callback = 0);
    // Load from file to D3DTexture, fallback to resource id, runs callback on completion
    void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, WORD id, AsyncLoadCallback callback = 0);
    // Load from file to D3DTexture, fallback to remote location, runs callback on completion
    void LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback = 0);

    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetProfessionIcon(GW::Constants::Profession p);
    // Fetches skill page from GWW, parses out the image for the skill then downloads that to disk
    // Not elegent, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetSkillImage(GW::Constants::SkillID skill_id);
    // Fetches item page from GWW, parses out the image for the item then downloads that to disk
    // Not elegent, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetItemImage(const std::wstring& item_name);
    // Fetches File page from GWW, parses out the image for the file given
    // Not elegent, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
    // Guaranteed to return a pointer, but reference will be null until the texture has been loaded
    static IDirect3DTexture9** GetGuildWarsWikiImage(const char* filename, size_t width = 0);

    // Guaranteed to return a pointer, but may not yet be decoded.
    GuiUtils::EncString* GetMapName(GW::Constants::MapID map_id);

    // Ensure file exists on disk, download from remote location if not found. If an error occurs, details are held in error string
    static void EnsureFileExists(const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback);

    // download to file, blocking. If an error occurs, details are held in response string
    bool Download(const std::filesystem::path& path_to_file, const std::string& url, std::wstring& response);
    // download to file, async, calls callback on completion. If an error occurs, details are held in response string
    void Download(const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback);
    // download to memory, blocking. If an error occurs, details are held in response string
    bool Download(const std::string& url, std::string& response);
    // download to memory, async, calls callback on completion. If an error occurs, details are held in response string
    void Download(const std::string& url, AsyncLoadMbCallback callback);

    // Enqueue instruction to be called on worker thread, away from the render loop e.g. curl requests
    void EnqueueWorkerTask(std::function<void()> f) { worker_mutex.lock(); thread_jobs.push(f); worker_mutex.unlock();}
    // Enqueue instruction to be called on the main update loop of GW
    void EnqueueMainTask(std::function<void()> f) { main_mutex.lock(); main_jobs.push(f); main_mutex.unlock(); }
    // Enqueue instruction to be called on the draw loop of GW e.g. messing with DirectX9 device
    void EnqueueDxTask(std::function<void(IDirect3DDevice9*)> f) { dx_mutex.lock(); dx_jobs.push(f); dx_mutex.unlock(); }

    // Stops the worker thread once it's done with the current jobs.
    void EndLoading();
private:

    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;
    // tasks to be done in the render thread
    std::queue<std::function<void(IDirect3DDevice9*)>> dx_jobs;
    // tasks to be done in main thread
    std::queue<std::function<void()>> main_jobs;

    bool should_stop = false;

    std::vector<std::thread*> workers;

    void Cleanup();
    // Assign IDirect3DTexture9* from file
    static HRESULT TryCreateTexture(IDirect3DDevice9* device, const std::filesystem::path& path_to_file, IDirect3DTexture9** texture, std::wstring& error);
    // Assign IDirect3DTexture9* from resource
    static HRESULT TryCreateTexture(IDirect3DDevice9* pDevice, HMODULE hSrcModule, LPCSTR pSrcResource, IDirect3DTexture9** texture, std::wstring& error);
    // Copy from compiled resource binary to file on local disk.
    bool ResourceToFile(WORD id, const std::filesystem::path& path_to_file, std::wstring& error);
};
