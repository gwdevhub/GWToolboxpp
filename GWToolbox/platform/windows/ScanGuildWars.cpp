#include "stdafx.h"

#include "Inject.h"
#include "ScanResult.h"
#include "Utf8.h"
#include "WindowsInject.h"

ScanResult ScanGuildWars()
{
    const InjectScanResult scan = RunScan();

    ScanResult result;
    for (const InjectProcess& process : scan.m_Processes) {
        ScanTarget target;
        target.kind = process.IsWasm() ? "wasm" : "native";
        target.pid = process.IsWasm() ? process.m_WasmSession->pid : process.m_Process.GetProcessId();
        target.character = ToUtf8(process.m_Charname);
        target.injected = process.m_Injected;
        result.targets.push_back(std::move(target));
    }
    if (!scan.m_Found && !scan.m_ErrorMessage.empty()) {
        result.errors.push_back({"", ToUtf8(scan.m_ErrorMessage)});
    }
    return result;
}
