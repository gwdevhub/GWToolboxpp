#include "Resources.h"

#include <Shlobj.h>
#include <Shlwapi.h>
#include <urlmon.h>
#include <WinInet.h>
#include <d3dx9tex.h>

#include <Defines.h>
#include "GWToolbox.h"
#include <logger.h>

void Resources::Initialize() {
	ToolboxModule::Initialize();
	worker = std::thread([this]() {
		while (!should_stop) {
			if (todo.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} else {
				todo.front()();
				todo.pop();
			}
		}
	});
}
void Resources::Terminate() {
	should_stop = true;
	if (worker.joinable()) worker.join();
}
void Resources::EndLoading() {
	todo.push([this]() { should_stop = true; });
}

bool Resources::GetPath(CHAR* path, const char* folder) const {
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) return false;
	PathAppend(path, "GWToolboxpp");
	if (folder) PathAppend(path, folder);
	return true;
}

void Resources::EnsureFullPathExists(const char* path) const {
	if (!PathFileExists(path)) {
		CreateDirectory(path, NULL);
	}
}

void Resources::EnsureSubPathExists(const char* sub) const {
	CHAR path[MAX_PATH];
	if (GetPath(path, sub)) {
		EnsureFullPathExists(path);
	}
}

void Resources::LoadTextureAsync(IDirect3DTexture9** texture, 
	const char* name, const char* folder, const char* url) {
	CHAR* path = new CHAR[MAX_PATH];
	if (!GetPath(path, folder)) return;
	EnsureFullPathExists(path);
	PathAppend(path, name);

	if (PathFileExists(path)) {
		toload.push([path, texture](IDirect3DDevice9* device) {
			D3DXCreateTextureFromFile(device, path, texture);
			delete[] path;
		});
	} else {
		CHAR* url_copy = new CHAR[MAX_PATH];
		strcpy_s(url_copy, MAX_PATH, url);
		todo.push([this, url_copy, path, texture]() {
			DeleteUrlCacheEntry(url_copy);
			Log::Log("Downloading %s\n", url_copy);
			if (URLDownloadToFile(NULL, url_copy, path, 0, NULL) == S_OK) {
				toload.push([path, texture](IDirect3DDevice9* device) {
					D3DXCreateTextureFromFile(device, path, texture);
					delete[] path;
				});
			} else {
				delete[] path;
				Log::Log("Error downloading %s\n", url_copy);
			}
			delete[] url_copy;
		});
	}
}

void Resources::EnsureFileExists(const char* name, 
	const char* folder, const char* url, std::function<void(bool)> callback) {

	CHAR* path = new CHAR[MAX_PATH];
	if (!GetPath(path, folder)) return;
	EnsureFullPathExists(path);
	PathAppend(path, name);

	if (PathFileExists(path)) {
		callback(true);
		delete[] path;
	} else {
		CHAR* url_copy = new CHAR[MAX_PATH];
		strcpy_s(url_copy, MAX_PATH, url);
		todo.push([this, url_copy, path, callback]() {
			DeleteUrlCacheEntry(url_copy);
			Log::Log("Downloading %s\n", url_copy);
			if (URLDownloadToFile(NULL, url_copy, path, 0, NULL) == S_OK) {
				callback(true);
			} else {
				callback(false);
				Log::Log("Error downloading %s\n", url_copy);
			}
			delete[] path;
			delete[] url_copy;
		});
	}
}

void Resources::DxUpdate(IDirect3DDevice9* device) {
	while (!toload.empty()) {
		toload.front()(device);
		toload.pop();
	}
}
