#include "stdafx.h"

#include "Cli.h"

#include "Install.h"
#include "Settings.h"
#include "Utf8.h"
#include "WebViewWindow.h"
#include "WindowsInject.h"

namespace {

void PrintJson(const Json& value)
{
    const std::string text = value.dump();
    fwrite(text.data(), 1, text.size(), stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

}

int PrintInstallStatus()
{
    const Json status = {
        {"installed", IsInstalled()},
        {"directory", ToUtf8(GetInstallationDir().wstring())},
    };
    if (settings.json) {
        PrintJson(status);
    }
    else {
        printf("installed: %s\ndirectory: %s\n", status["installed"].get<bool>() ? "yes" : "no", status["directory"].get<std::string>().c_str());
    }
    return 0;
}

int PrintScanResult()
{
    const InjectScanResult scan = RunScan();

    Json processes = Json::array();
    for (const InjectProcess& target : scan.m_Processes) {
        Json entry = {
            {"kind", target.IsWasm() ? "wasm" : "native"},
            {"injected", target.m_Injected},
            {"character", ToUtf8(target.m_Charname)},
        };
        if (!target.IsWasm()) {
            entry["pid"] = target.m_Process.GetProcessId();
        }
        processes.push_back(std::move(entry));
    }

    Json result = {
        {"found", scan.m_Found},
        {"processes", processes},
    };
    if (!scan.m_ErrorMessage.empty()) {
        result["error"] = ToUtf8(scan.m_ErrorMessage);
    }

    if (settings.json) {
        PrintJson(result);
    }
    else {
        for (const Json& entry : processes) {
            printf("%s pid=%s injected=%s character=%s\n",
                   entry["kind"].get<std::string>().c_str(),
                   entry.contains("pid") ? entry["pid"].dump().c_str() : "-",
                   entry["injected"].get<bool>() ? "yes" : "no",
                   entry["character"].get<std::string>().c_str());
        }
        if (result.contains("error")) {
            fprintf(stderr, "%s\n", result["error"].get<std::string>().c_str());
        }
    }
    return scan.m_Found ? 0 : 1;
}

void PrintInjectResult(const bool ok, const std::wstring& message)
{
    PrintJson(Json{{"ok", ok}, {"message", ToUtf8(message)}});
}
