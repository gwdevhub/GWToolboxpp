#include "stdafx.h"

#include <shobjidl.h>

#include <optional>
#include <string>

#include "FileDialog.h"
#include "Utf8.h"

namespace {

std::optional<std::string> showOpenDialog(const std::string& start, const FILEOPENDIALOGOPTIONS extra_options, const COMDLG_FILTERSPEC* filter, const UINT filter_count)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return std::nullopt;
    }

    std::optional<std::string> picked;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        FILEOPENDIALOGOPTIONS options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_FORCEFILESYSTEM | extra_options);
        }
        if (filter) {
            dialog->SetFileTypes(filter_count, filter);
        }

        const std::wstring start_wide = FromUtf8(start);
        IShellItem* folder = nullptr;
        if (!start_wide.empty() && SUCCEEDED(SHCreateItemFromParsingName(start_wide.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    picked = ToUtf8(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    CoUninitialize();
    return picked;
}

}

std::optional<std::string> ChooseDllFile(const std::string& start)
{
    constexpr COMDLG_FILTERSPEC filters[] = {{L"GWToolbox DLL", L"GWToolboxdll.dll;GWToolbox.dll"}, {L"DLL files", L"*.dll"}};
    return showOpenDialog(start, FOS_FILEMUSTEXIST, filters, _countof(filters));
}

std::optional<std::string> ChooseFolder(const std::string& start)
{
    return showOpenDialog(start, FOS_PICKFOLDERS, nullptr, 0);
}
