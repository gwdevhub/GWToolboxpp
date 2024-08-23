#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <base64.h>
#include <ctime>
#include <regex>

#include <Modules/GameSettings.h>
#include <Modules/ToolboxCommandReceiverModule.h>

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <Constants/EncStrings.h>

#include <fstream>
#include <iostream>
#include <Windows.h>

using nlohmann::json;
namespace {
    HANDLE hMapFile;
    LPVOID pBuf;
    HANDLE hSemaphore;
    std::string file_name = "toolbox_mmf";
    std::string semaphore_name = "toolbox_sem";
    int map_size = 1000000; 
    size_t last_position = 0;
    bool listening = false;

    void ListenForNewLines()
    {
        listening = true;
        while (listening) {
            // Read from the memory-mapped file
            auto* data = (char*)pBuf + last_position;
            auto* end = (char*)pBuf + map_size;

            while (data < end && *data != '\0') {
                if (*data == '\n') {
                    const auto lineLength = data - ((char*)pBuf + last_position);
                    std::string line((char*)pBuf + last_position, lineLength);
                    Log::Error("%s", line.c_str());
                    last_position = data - (char*)pBuf + 1;
                }
                data++;
            }

            Sleep(100); // Small delay to prevent busy-waiting
        }
    }

    void CreateMMF()
    {
        // Create or open the named semaphore with an initial count of 0 (which will be incremented later)
        hSemaphore = CreateSemaphore(NULL, 0, LONG_MAX, semaphore_name.c_str());
        if (hSemaphore == NULL) {
            std::cerr << "CreateSemaphore error: " << GetLastError() << std::endl;
            exit(1);
        }

        // Increment the semaphore count to indicate this process is using the resource
        ReleaseSemaphore(hSemaphore, 1, NULL);

        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE, // Use paging file
            NULL,                 // Default security
            PAGE_READWRITE,       // Read/write access
            0,                    // Maximum object size (high-order DWORD)
            map_size,             // Maximum object size (low-order DWORD)
            file_name.c_str()     // Name of the mapping object
        );

        if (hMapFile == NULL) {
            std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
            WaitForSingleObject(hSemaphore, INFINITE);
            CloseHandle(hSemaphore);
            exit(1);
        }

        pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (pBuf == NULL) {
            std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
            WaitForSingleObject(hSemaphore, INFINITE);
            CloseHandle(hMapFile);
            CloseHandle(hSemaphore);
            exit(1);
        }
    }

    void CleanupMMF()
    {
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        DeleteFile(file_name.c_str());
        std::cout << "Cleaned up memory-mapped file." << std::endl;
    }

    void RegisterToolboxProtocol()
    {
        // Define registry key path and values
        const std::wstring protocolKey = L"toolbox";
        const std::wstring shellKey = L"toolbox\\shell\\open\\command";
        const std::wstring command = L"powershell.exe -Command \""
                                     L"$filePath = 'toolbox_mmf'; "
                                     L"$mapSize = 1000000; "
                                     L"try { "
                                     L"    $mapFile = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($filePath); "
                                     L"    $viewAccessor = $mapFile.CreateViewAccessor(); "
                                     L"    $position = 0; "
                                     L"    while ($position -lt $mapSize -and $viewAccessor.ReadByte($position) -ne 0) { $position++ } "
                                     L"    $urlBytes = [System.Text.Encoding]::UTF8.GetBytes('%1'); " // Write the URL first
                                     L"    $viewAccessor.WriteArray($position, $urlBytes, 0, $urlBytes.Length); "
                                     L"    $position += $urlBytes.Length; "              // Move the position after the URL
                                     L"    $viewAccessor.Write($position, [byte]0x0A); " // Write the newline character separately
                                     L"    $viewAccessor.Flush(); "
                                     L"    $viewAccessor.Dispose(); "
                                     L"    $mapFile.Dispose(); "
                                     L"} catch { "
                                     L"    Write-Output 'Memory-mapped file does not exist or cannot be accessed. Skipping write operation.'; "
                                     L"}\"";


        // Open or create the protocol key in HKEY_CLASSES_ROOT
        HKEY hKey;
        if (RegCreateKeyExW(HKEY_CLASSES_ROOT, protocolKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            // Set the default value for the protocol key
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)L"URL:Toolbox Protocol", (DWORD)(wcslen(L"URL:Toolbox Protocol") * sizeof(wchar_t)));

            // Set the URL Protocol value to indicate that this is a URL protocol
            RegSetValueExW(hKey, L"URL Protocol", 0, REG_SZ, (const BYTE*)L"", 0);

            // Close the protocol key
            RegCloseKey(hKey);
        }

        // Open or create the shell\open\command key in HKEY_CLASSES_ROOT
        if (RegCreateKeyExW(HKEY_CLASSES_ROOT, shellKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            // Set the command value to execute the PowerShell command
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)(command.length() * sizeof(wchar_t)));

            // Close the command key
            RegCloseKey(hKey);
        }
    }

    void UnregisterToolboxProtocol()
    {
        const std::wstring protocolKey = L"toolbox";
        const std::wstring shellKey = L"toolbox\\shell";

        RegDeleteTreeW(HKEY_CLASSES_ROOT, shellKey.c_str());
        RegDeleteTreeW(HKEY_CLASSES_ROOT, protocolKey.c_str());
        std::cout << "Unregistered protocol." << std::endl;
    }

    void TerminateAndCleanup() {
        listening = false;
        LONG prevCount;
        ReleaseSemaphore(hSemaphore, 1, &prevCount);
        WaitForSingleObject(hSemaphore, INFINITE);
        if (prevCount <= 1) {
            CleanupMMF();
            UnregisterToolboxProtocol();
        }

        WaitForSingleObject(hSemaphore, INFINITE);
        CloseHandle(hSemaphore);
    }
}

void ToolboxCommandsModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterToolboxProtocol();
    CreateMMF();
    std::thread listenerThread(ListenForNewLines);
    listenerThread.detach();
}

void ToolboxCommandsModule::Terminate()
{
    ToolboxModule::Terminate();
    TerminateAndCleanup();
}

void ToolboxCommandsModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
}

void ToolboxCommandsModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
}

void ToolboxCommandsModule::RegisterSettingsContent() {
}
