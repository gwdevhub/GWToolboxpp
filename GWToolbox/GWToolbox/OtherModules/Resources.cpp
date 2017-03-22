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

bool Resources::GetURL(CHAR* url, const char* name, const char* folder) const {
	// todo: find a better way that doesn't break if folder has a trailing slash
	if (folder) {
		sprintf_s(url, MAX_PATH, "%s%s/%s", GWTOOLBOX_HOST, folder, name);
	} else {
		sprintf_s(url, MAX_PATH, "%s%s", GWTOOLBOX_HOST, name);
	}
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

void Resources::LoadTextureAsync(IDirect3DTexture9** texture, const char* name, const char* folder) {
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
		CHAR* url = new CHAR[MAX_PATH];
		GetURL(url, name, folder);
		todo.push([this, url, path, texture]() {
			DeleteUrlCacheEntry(url);
			Log::Log("Downloading %s\n", url);
			if (URLDownloadToFile(NULL, url, path, 0, NULL) == S_OK) {
				toload.push([path, texture](IDirect3DDevice9* device) {
					D3DXCreateTextureFromFile(device, path, texture);
					delete[] path;
				});
			} else {
				delete[] path;
				Log::Log("Error downloading %s\n", url);
			}
			delete[] url;
		});
	}
}

void Resources::EnsureFileExists(const char* name, 
	const char* folder, std::function<void()> callback) {

	CHAR* path = new CHAR[MAX_PATH];
	if (!GetPath(path, folder)) return;
	EnsureFullPathExists(path);
	PathAppend(path, name);

	if (PathFileExists(path)) {
		callback();
		delete[] path;
	} else {
		CHAR* url = new CHAR[MAX_PATH];
		GetURL(url, name, folder);
		todo.push([this, url, path, callback]() {
			DeleteUrlCacheEntry(url);
			Log::Log("Downloading %s\n", url);
			if (URLDownloadToFile(NULL, url, path, 0, NULL) == S_OK) {
				callback();
			} else {
				Log::Log("Error downloading %s\n", url);
			}
			delete[] url;
			delete[] path;
		});
	}
}

void Resources::Draw(IDirect3DDevice9* device) {
	while (!toload.empty()) {
		toload.front()(device);
		toload.pop();
	}
}
