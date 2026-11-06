#pragma once

#include <ToolboxModule.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class GuildWarsFileServerModule : public ToolboxModule {
    GuildWarsFileServerModule() = default;

public:
    enum class State { NotRequested, Pending, Ready, Failed };

    struct File {
        uint32_t file_id = 0;
        std::vector<uint8_t> data;
    };

    using FilePtr = std::shared_ptr<const File>;
    using Callback = std::function<void(FilePtr file, const std::string& error)>;

    static GuildWarsFileServerModule& Instance()
    {
        static GuildWarsFileServerModule instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars File Server"; }
    bool HasSettings() override { return false; }
    void Initialize() override;
    void Update(float) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;

    static bool FetchFile(uint32_t base_file_id, Callback callback = nullptr);
    static State GetFile(uint32_t base_file_id, FilePtr& file);
};
