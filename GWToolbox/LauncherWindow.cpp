#include "stdafx.h"

#include "LauncherWindow.h"

namespace {

std::string LoadResourceText(const int id)
{
    const auto module = GetModuleHandleW(nullptr);
    const auto info = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (info == nullptr) {
        fprintf(stderr, "Launcher page resource %d not found (%lu)\n", id, GetLastError());
        return {};
    }
    const auto data = static_cast<const char*>(LockResource(LoadResource(module, info)));
    return {data, SizeofResource(module, info)};
}

void InlineResource(std::string& page, const std::string_view token, const int id)
{
    const auto at = page.find(token);
    if (at != std::string::npos) {
        page.replace(at, token.size(), LoadResourceText(id));
    }
}

WindowOptions MakeOptions(std::string title, const int width, const int height)
{
    WindowOptions options;
    options.title = std::move(title);
    options.width = width;
    options.height = height;
    options.resizable = false;
    options.transparent = false;
    if (const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA")) {
        options.webViewDataFolder = std::filesystem::path(local_app_data) / L"GWToolbox" / L"WebView2";
    }
    return options;
}

}

bool StartLauncherWindowing()
{
    static const bool started = [] {
        setApplicationName("GWToolbox");
        try {
            initializeWindowing();
            return true;
        }
        catch (const RuntimeMissingError&) {
            return false;
        }
        catch (const std::exception& error) {
            showError(error.what());
            return false;
        }
    }();
    return started;
}

void PumpMessagesFor(const std::uint32_t timeout_ms)
{
    MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout_ms, QS_ALLINPUT);

    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            std::exit(static_cast<int>(message.wParam));
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void PumpMessagesUntil(const std::function<bool()>& done)
{
    while (!done()) {
        PumpMessagesFor(16);
    }
}

std::string ToUtf8(const std::wstring_view text)
{
    const auto length = static_cast<int>(text.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}

void showError(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
    MessageBoxA(nullptr, message.c_str(), applicationName().c_str(), MB_OK | MB_ICONERROR);
}

LauncherWindow::LauncherWindow(std::string title, const int width, const int height, const int page_resource_id)
    : WebViewWindow(MakeOptions(std::move(title), width, height))
    , page_resource_id_(page_resource_id)
{
    handler().on("close", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.close();
        reply.result(nullptr);
    });
    handler().on("beginMove", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
}

std::string LauncherWindow::html() const
{
    std::string page = LoadResourceText(page_resource_id_);
    InlineResource(page, "<!--launcher.css-->", IDR_LAUNCHER_CSS);
    InlineResource(page, "<!--launcher.js-->", IDR_LAUNCHER_JS);
    return page;
}
