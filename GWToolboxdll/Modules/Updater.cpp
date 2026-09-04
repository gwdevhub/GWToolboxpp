#include "stdafx.h"

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/BackupModule.h>
#include <Modules/Resources.h>
#include <Modules/Updater.h>

namespace github_api {
    struct ReleaseAsset {
        std::string name;
        std::string browser_download_url;
        double size = 0.0; // 字节（double 以承受 53 位 JSON 精度）
    };

    struct Release {
        std::string tag_name;
        std::optional<std::string> body; // 当发布没有描述文本时 Github 返回 null
        bool prerelease = false;
        std::vector<ReleaseAsset> assets;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    using ReleaseType = Updater::ReleaseType;
    using Mode = Updater::Mode;

    Updater::Settings settings;

    // 0=检查中, 1=询问中, 2=下载中, 3=完成
    enum Step {
        Checking,
        CheckAndAsk,
        CheckAndWarn,
        CheckAndAutoUpdate,
        Downloading,
        Success,
        Done
    };

    Step step = Checking;

    bool is_latest_version = true;
    bool notified = false;
    bool forced_ask = false;
    clock_t last_check = 0;

    // 在启动时设置一次，当运行版本与我们上次保存的版本不同时 —
    // 即工具箱刚刚更新。驱动一次性星标请求。
    bool show_star_request = false;

    GWToolboxRelease latest_release;
    GWToolboxRelease current_release;

    GWToolboxRelease* GetLatestRelease(GWToolboxRelease* release)
    {
        std::string response;
        unsigned int tries = 0;
        //const auto url = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases";
        const auto url = "https://api.github.com/repos/coolnovor/GWToolboxpp/releases";
        bool success = false;
        do {
            success = Resources::Instance().Download(url, response);
            tries++;
        } while (!success && tries < 5);
        if (!success) {
            Log::Log("下载 %s 失败\n%s", url, response.c_str());
            return nullptr;
        }
        std::vector<github_api::Release> releases;
        if (auto ec = glz::read<json_opts>(releases, response); ec) {
            return nullptr;
        }
        for (const auto& js : releases) {
            if (js.tag_name.empty() || js.assets.empty()) {
                continue;
            }
            if (js.prerelease && settings.update_release_type == ReleaseType::Stable) {
                continue;
            }
            const auto version_number_len = js.tag_name.find(js.tag_name.contains("_Release") ? "_Release" : "_Beta", 0);
            if (version_number_len == std::string::npos) {
                continue;
            }
            for (const auto& asset : js.assets) {
                if (asset.name != "GWToolbox.dll" && asset.name != "GWToolboxdll.dll") {
                    continue; // 此发布没有 dll 下载。
                }
                release->download_url = asset.browser_download_url;
                release->version = js.tag_name.substr(0, version_number_len);
                if (js.prerelease) {
                    release->version += js.tag_name.substr(version_number_len + 1);
                }
                std::ranges::transform(release->version, release->version.begin(), [](const auto chr) { return static_cast<char>(std::tolower(chr)); });
                release->body = js.body.value_or("");
                const auto size_bytes = static_cast<uintmax_t>(asset.size); // 略微舍入，GitHub 不总是精确到字节。
                release->size = static_cast<uintmax_t>(std::ceil(size_bytes / 16.0) * 16);
                return release;
            }
        }
        return nullptr;
    }

    char update_available_text[128];

    const char* UpdateAvailableText()
    {
        int written = 0;
        if (latest_release.version == current_release.version && latest_release.size != current_release.size) {
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s (%.2f kb) is available! You have %s (%.2f kb)",
                               latest_release.version.c_str(), latest_release.size > 0 ? latest_release.size / 1024.f : 0.f,
                               current_release.version.c_str(), current_release.size > 0 ? current_release.size / 1024.f : 0.f);
        }
        else {
            written = snprintf(update_available_text, sizeof(update_available_text) - 1, "GWToolbox++ version %s is available! You have %s", latest_release.version.c_str(), current_release.version.c_str());
        }
        ASSERT(written > 0);
        return update_available_text;
    }

    void DoUpdate()
    {
        Log::Warning("更新前创建设置备份...");
        if (!BackupModule::CreateAutoBackup())
            Log::Warning("创建更新前备份失败；继续更新。");

        Log::Warning("正在下载更新...");

        step = Downloading;

        // 0. 查找工具箱 dll 路径
        const HMODULE module = GWToolbox::GetDLLModule();
        WCHAR dllfile[MAX_PATH];
        const DWORD size = GetModuleFileNameW(module, dllfile, MAX_PATH);
        if (size == 0) {
            Log::Error("更新程序错误 - 找不到 GWToolbox.dll 路径");
            step = Done;
            return;
        }
        Log::Log("dll 文件名为 %s\n", dllfile);

        const std::wstring dll_path(dllfile);
        std::wstring dll_name;
        wchar_t sep = '/';
#ifdef _WIN32
        sep = '\\';
#endif

        const size_t i = dll_path.rfind(sep, dll_path.length());
        if (i != std::wstring::npos) {
            dll_name = dll_path.substr(i + 1, dll_path.length() - i);
        }
        if (dll_name.empty()) {
            Log::Error("更新程序错误 - 从路径提取 dll 名称失败");
            step = Done;
            return;
        }

        // 1. 重命名工具箱 dll
        const auto dllold = std::wstring(dllfile) + L".old";
        Log::Log("移动到 %s\n", dllold.c_str());
        DeleteFileW(dllold.c_str());
        MoveFileW(dllfile, dllold.c_str());

        // 2. 下载新 dll
        Resources::Instance().Download(
            dllfile, latest_release.download_url,
            [wdll = std::wstring(dllfile), dllold](const bool success, const std::wstring& error) -> void {
                if (success) {
                    step = Success;
                    Log::WarningW(L"更新成功，请重启工具箱。");
                }
                else {
                    Log::ErrorW(L"更新错误 - 无法下载 GWToolbox.dll\n%s", error.c_str());
                    MoveFileW(dllold.c_str(), wdll.c_str());
                    step = Done;
                }
            });
    }

    void DrawStarRequest()
    {
        if (!show_star_request) {
            return;
        }
        bool keep_open = true;
        ImGui::SetNextWindowSize(ImVec2(440.0f * ImGui::FontScale(), -1), ImGuiCond_Appearing);
        ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
        if (ImGui::Begin("感谢您更新 GWToolbox++！###gwtoolbox_star_request", &keep_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(
                "GWToolbox++ 由一小群志愿者在业余时间构建和维护，并免费提供。这一点永远不会改变。");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "有一件小事您可以做，它确实能帮助我们。因为工具箱必须注入到激战中并读取其内存，"
                "杀毒软件经常将其标记为误报。一个拥有大量星标和稳定活动的 GitHub 项目在那些厂商看来"
                "要合法得多，久而久之，这意味着每个玩家的误报都会减少。");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "星标还能帮助我们获得维持和改进工具箱所需的免费开发者工具——比如 JetBrains 的开源许可证"
                "最近收紧了要求，而项目活动是他们决定的一部分。");
            ImGui::Spacing();
            ImGui::TextUnformatted(
                "所以如果工具箱对您有用，您愿意花几秒钟在 GitHub 上给我们一个星标吗？"
                "它是完全免费的，能帮助我们在杀毒软件中的声誉，而且说实话——每次收到星标都让我们一整天都开心。");
            ImGui::Spacing();
            ImGui::TextUnformatted("感谢您成为这个社区的一部分。<3");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Checkbox("I've already starred###gwtoolbox_has_starred", &settings.has_starred)) {
                show_star_request = false;
            }
            ImGui::Spacing();
            if (ImGui::Button("Star us on GitHub###gwtoolbox_open_star", ImVec2(200.0f * ImGui::FontScale(), 0))) {
                ShellExecute(nullptr, "open", "https://github.com/gwdevhub/GWToolboxpp", nullptr, nullptr, SW_SHOWNORMAL);
                show_star_request = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("稍后再说###gwtoolbox_dismiss_star", ImVec2(120.0f * ImGui::FontScale(), 0))) {
                show_star_request = false;
            }
        }
        ImGui::End();
        if (!keep_open) {
            show_star_request = false;
        }
    }
}

const std::string& Updater::GetServerVersion()
{
    return latest_release.version;
}

const GWToolboxRelease* Updater::GetCurrentVersionInfo(GWToolboxRelease* out)
{
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(GWToolbox::GetDLLModule(), path, _countof(path)) == 0) {
        return nullptr;
    }
    auto size_bytes = std::filesystem::file_size(path);
    out->size = static_cast<uintmax_t>(std::ceil(size_bytes / 16.0) * 16);
    out->version = GWTOOLBOXDLL_VERSION;
    out->version.append(GWTOOLBOXDLL_VERSION_BETA);
    std::ranges::transform(out->version, out->version.begin(), [](const auto chr) {
        return static_cast<char>(std::tolower(chr));
    });
    return out;
}

void Updater::Initialize()
{
    ToolboxUIElement::Initialize();
#ifndef _DEBUG
    // 调试版本从不加载/保存更新设置（使用强制值），因此不注册它们
    SettingsRegistry::Register(this, settings);
#endif
}

void Updater::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxUIElement::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
#ifdef _DEBUG
    settings.update_mode = Mode::DontCheckForUpdates;
    settings.update_release_type = ReleaseType::Beta;
#else
    std::string previous_version;
    if (doc.Get(Name(), "dllversion", previous_version) && !previous_version.empty() && previous_version != GWTOOLBOXDLL_VERSION && !settings.has_starred) {
        show_star_request = true;
    }
#endif
    CheckForUpdate();
}

void Updater::SaveSettings(SettingsDoc& doc)
{
    ToolboxUIElement::SaveSettings(doc);
#ifndef _DEBUG
    doc.SetStruct(Name(), settings);
    doc.Set(Name(), "dllversion", std::string(GWTOOLBOXDLL_VERSION));

    const HMODULE module = GWToolbox::GetDLLModule();
    CHAR dllfile[MAX_PATH];
    const DWORD size = GetModuleFileName(module, dllfile, MAX_PATH);
    doc.Set(Name(), "dllpath", std::string(size > 0 ? dllfile : "error"));
#endif
}

void Updater::DrawSettingsInternal()
{
    ImGui::Text("发布频道：");
    const float btnWidth = 180.0f * ImGui::FontScale();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnWidth);
    if (ImGui::Button(step == Checking ? "检查中..." : "检查更新", ImVec2(btnWidth, 0)) && step != Checking) {
        CheckForUpdate(true);
    }
    ImGui::RadioButton("稳定版", (int*)&settings.update_release_type, static_cast<int>(ReleaseType::Stable));
    ImGui::RadioButton("测试版", (int*)&settings.update_release_type, static_cast<int>(ReleaseType::Beta));
    ImGui::Text("更新模式：");
    ImGui::RadioButton("不检查更新", (int*)&settings.update_mode, static_cast<int>(Mode::DontCheckForUpdates));
    ImGui::RadioButton("检查并显示消息", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndWarn));
    ImGui::RadioButton("检查并询问后更新", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndAsk));
    ImGui::RadioButton("检查并自动更新", (int*)&settings.update_mode, static_cast<int>(Mode::CheckAndAutoUpdate));
}

void Updater::CheckForUpdate(const bool forced)
{
    if (!GetCurrentVersionInfo(&current_release)) {
        Log::Error("获取当前工具箱版本信息失败");
    }
    step = Checking;
    last_check = clock();
    Resources::EnqueueWorkerTask([forced] {
        // 这里在工作线程中，可以进行阻塞操作
        // 提醒：不要从此线程向 GW 聊天发送内容！
        if (!GetLatestRelease(&latest_release)) {
            // 获取服务器版本出错。服务器宕机？我们无能为力。
            Log::Flash("检查更新时出错");
            step = Done;
            return;
        }

        if (latest_release.version == current_release.version
            && latest_release.size == current_release.size) {
            step = Done;
            is_latest_version = true;
            if (forced) {
                Log::Flash("GWToolbox++ 已是最新版本");
            }
            return;
        }
        is_latest_version = false;
        if (!forced && settings.update_mode == Mode::DontCheckForUpdates) {
            step = Done;
            return; // 不检查更新
        }

        // 有新版本！
        Mode iMode = forced ? Mode::CheckAndAsk : settings.update_mode;
        if constexpr (!std::string_view(GWTOOLBOXDLL_VERSION_BETA).empty()) {
            iMode = Mode::CheckAndAsk;
        }
        switch (iMode) {
            case Mode::CheckAndAsk:
                step = CheckAndAsk;
                break;
            case Mode::CheckAndAutoUpdate:
                step = CheckAndAutoUpdate;
                break;
            case Mode::CheckAndWarn:
                step = CheckAndWarn;
                break;
        }
    });
}

bool Updater::IsLatestVersion()
{
    return is_latest_version;
}

void Updater::Draw(IDirect3DDevice9*)
{
    DrawStarRequest();
    switch (step) {
        case CheckAndWarn:
            Log::Warning(UpdateAvailableText());
            step = Done;
            break;
        case CheckAndAsk: {
            if (!visible) {
                visible = true;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("工具箱更新！", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("更新内容：");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::TextUnformatted("\n是否要更新？");
            if (ImGui::Button("稍后###gwtoolbox_dont_update", ImVec2(100, 0))) {
                step = Done;
            }
            ImGui::SameLine();
            if (ImGui::Button("确定###gwtoolbox_do_update", ImVec2(100, 0))) {
                DoUpdate();
            }
            ImGui::End();
            if (!visible) {
                step = Done;
            }
        }
        break;
        case CheckAndAutoUpdate:
            DoUpdate();
            break;
        case Downloading: {
            if (!visible) {
                break;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("工具箱更新！", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("更新内容：");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::Text("\n正在下载更新...");
            if (ImGui::Button("隐藏", ImVec2(100, 0))) {
                visible = false;
            }
            ImGui::End();
        }
        break;
        case Success: {
            if (!visible) {
                break;
            }
            ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiCond_Appearing);
            ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
            ImGui::Begin("工具箱更新！", &visible);
            ImGui::TextUnformatted(UpdateAvailableText());
            ImGui::TextUnformatted("更新内容：");
            ImGui::TextUnformatted(latest_release.body.c_str());
            ImGui::Text("\n更新成功，请重启工具箱。");
            if (ImGui::Button("确定", ImVec2(100, 0))) {
                visible = false;
            }
            if (!visible) {
                step = Done;
            }
            ImGui::End();
        }
        break;
    }
    // 如果 step == Done 则不做任何事
}
