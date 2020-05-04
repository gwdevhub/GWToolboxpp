#include "stdafx.h"
#include "Resources.h"

#include <thread>

#include <urlmon.h>
#include <WinInet.h>
#include <d3dx9_dynamic.h>

#include <Defines.h>
#include "GWToolbox.h"
#include <logger.h>

void Resources::Initialize() {
	ToolboxModule::Initialize();
	worker = std::thread([this]() {
		while (!should_stop) {
			if (thread_jobs.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} else {
				thread_jobs.front()();
				thread_jobs.pop();
			}
		}
	});
}
void Resources::Terminate() {
	should_stop = true;
	if (worker.joinable()) worker.join();
}
void Resources::EndLoading() {
	thread_jobs.push([this]() { should_stop = true; });
}

std::wstring Resources::GetSettingsFolderPath() {
	WCHAR path[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
	PathAppendW(path, L"GWToolboxpp");
	return std::wstring(path);
}
std::wstring Resources::GetPath(std::wstring file) {
	return GetSettingsFolderPath() + L"\\" + file;
}
std::wstring Resources::GetPath(std::wstring folder, std::wstring file) {
	return GetSettingsFolderPath() + L"\\" + folder + L"\\" + file;
}

void Resources::EnsureFolderExists(std::wstring path) {
	if (!PathFileExistsW(path.c_str())) {
		CreateDirectoryW(path.c_str(), NULL);
	}
}

utf8::string Resources::GetPathUtf8(std::wstring file) {
	std::wstring path = GetPath(file);
	return Unicode16ToUtf8(path.c_str());
}

bool Resources::Download(std::wstring path_to_file, std::wstring url) {
	DeleteUrlCacheEntryW(url.c_str());
	Log::Log("Downloading %s\n", url.c_str());
	return (URLDownloadToFileW(NULL, url.c_str(), path_to_file.c_str(), 0, NULL) == S_OK);
}
void Resources::Download(std::wstring path_to_file, 
	std::wstring url, std::function<void(bool)> callback) {

	thread_jobs.push([this, url, path_to_file, callback]() {
		bool success = Download(path_to_file, url);
		// and call the callback in the main thread
		todo.push([callback, success]() { callback(success); });
		if (!success) {
			Log::Log("Error downloading %s\n", url.c_str());
		}
	});
}

std::string Resources::Download(std::wstring url) const {
	DeleteUrlCacheEntryW(url.c_str());
	IStream* stream;
	std::string ret = "";
	if (SUCCEEDED(URLOpenBlockingStreamW(NULL, url.c_str(), &stream, 0, NULL))) {
		STATSTG stats;
		stream->Stat(&stats, STATFLAG_NONAME);
		DWORD size = stats.cbSize.LowPart;
		CHAR* chars = new CHAR[size + 1];
		stream->Read(chars, size, NULL);
		chars[size] = '\0';
		ret = std::string(chars);
		delete[] chars;
		stream->Release();
	}
	return ret;
}
void Resources::Download(std::wstring url, std::function<void(std::string)> callback) {
	thread_jobs.push([this, url, callback]() {
		std::string s = Download(url);
		todo.push([callback, s]() { callback(s); });
	});
}

void Resources::EnsureFileExists(std::wstring path_to_file, 
	std::wstring url, std::function<void(bool)> callback) {

	if (PathFileExistsW(path_to_file.c_str())) {
		// if file exists, run the callback immediately in the same thread
		callback(true);
	} else {
		// otherwise try to download it in the worker
		Download(path_to_file, url, callback);
	}
}

void Resources::LoadTextureAsync(IDirect3DTexture9** texture,
	std::wstring path_to_file) {

	if (PathFileExistsW(path_to_file.c_str())) {
		toload.push([path_to_file, texture](IDirect3DDevice9* device) {
			D3DXCreateTextureFromFileW(device, path_to_file.c_str(), texture);
		});
	}
}

void Resources::LoadTextureAsync(IDirect3DTexture9** texture, 
	std::wstring path_to_file, WORD id) {

	if (PathFileExistsW(path_to_file.c_str())) {
		// if file exists load it
		toload.push([path_to_file, texture](IDirect3DDevice9* device) {
			D3DXCreateTextureFromFileW(device, path_to_file.c_str(), texture);
		});
	} else if (id > 0) {
		// otherwise try to install it from resource
		HRSRC hResInfo = FindResource(GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), RT_RCDATA);
		HGLOBAL hRes = LoadResource(GWToolbox::GetDLLModule(), hResInfo);
		DWORD size = SizeofResource(GWToolbox::GetDLLModule(), hResInfo);

		// write to file so the user can customize his icons
		HANDLE hFile = CreateFileW(path_to_file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD bytesWritten;
		BOOL wfRes = WriteFile(hFile, hRes, size, &bytesWritten, NULL);
		if (wfRes != TRUE) {
			DWORD wfErr = GetLastError();
			Log::Error("Error writing file %s - Error is %lu", path_to_file.c_str(), wfErr);
		} else if (bytesWritten != size) {
			Log::Error("Wrote %lu of %lu bytes for %s", bytesWritten, size, path_to_file.c_str());
		}
		CloseHandle(hFile);
		// Note: this WILL fail for some users. Don't care, it's only needed for customization.

		// finally load the texture from the resource
		toload.push([id, texture](IDirect3DDevice9* device) {
			HRESULT res = D3DXCreateTextureFromResource(device, GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), texture);
		});
	}
}

void Resources::DxUpdate(IDirect3DDevice9* device) {
	while (!toload.empty()) {
		toload.front()(device);
		toload.pop();
	}
}

void Resources::Update(float delta) {
	while (!todo.empty()) {
		todo.front()();
		todo.pop();
	}
}
