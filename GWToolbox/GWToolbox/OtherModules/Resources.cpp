#include "Resources.h"

#include <Shlobj.h>
#include <Shlwapi.h>
#include <urlmon.h>
#include <WinInet.h>
#include <d3dx9tex.h>

#include <Defines.h>
#include "GWToolbox.h"

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

void Resources::LoadTextureAsync(IDirect3DTexture9** texture, const char* name, const char* folder) {
	CHAR* path = new CHAR[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) return;
	PathAppend(path, "GWToolboxpp");
	if (folder) PathAppend(path, folder);
	if (!PathFileExists(path)) {
		CreateDirectory(path, NULL);
	}
	PathAppend(path, name);

	if (PathFileExists(path)) {
		toload.push([path, texture](IDirect3DDevice9* device) {
			D3DXCreateTextureFromFile(device, path, texture);
			delete[] path;
		});
	} else {
		CHAR* url = new CHAR[MAX_PATH];
		if (folder) {
			sprintf_s(url, MAX_PATH, "%s%s/%s", GWTOOLBOX_HOST, folder, name);
		} else {
			sprintf_s(url, MAX_PATH, "%s%s", GWTOOLBOX_HOST, name);
		}
		todo.push([this, url, path, texture]() {
			DeleteUrlCacheEntry(url);
			printf("Downloading %s\n", url);
			if (URLDownloadToFile(NULL, url, path, 0, NULL) == S_OK) {
				toload.push([path, texture](IDirect3DDevice9* device) {
					D3DXCreateTextureFromFile(device, path, texture);
					delete[] path;
				});
			}
			delete[] url;
		});
	}
}

void Resources::Draw(IDirect3DDevice9* device) {
	while (!toload.empty()) {
		toload.front()(device);
		toload.pop();
	}
}
