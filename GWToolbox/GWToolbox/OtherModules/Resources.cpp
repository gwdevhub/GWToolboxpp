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

std::string Resources::GetSettingsFolderPath() {
	CHAR path[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
	PathAppend(path, "GWToolboxpp");
	return std::string(path);

}
std::string Resources::GetPath(std::string file) {
	return GetSettingsFolderPath() + "\\" + file;
}
std::string Resources::GetPath(std::string folder, std::string file) {
	return GetSettingsFolderPath() + "\\" + folder + "\\" + file;
}

void Resources::EnsureFolderExists(std::string path) {
	if (!PathFileExists(path.c_str())) {
		CreateDirectory(path.c_str(), NULL);
	}
}

bool Resources::Download(std::string path_to_file, std::string url) const {
	DeleteUrlCacheEntry(url.c_str());
	Log::Log("Downloading %s\n", url.c_str());
	return (URLDownloadToFile(NULL, url.c_str(), path_to_file.c_str(), 0, NULL) == S_OK);
}
void Resources::Download(std::string path_to_file, 
	std::string url, std::function<void(bool)> callback) {

	thread_jobs.push([this, url, path_to_file, callback]() {
		bool success = Download(path_to_file, url);
		// and call the callback in the main thread
		todo.push([callback, success]() { callback(success); });
		if (!success) {
			Log::Log("Error downloading %s\n", url.c_str());
		}
	});
}

std::string Resources::Download(std::string url) const {
	DeleteUrlCacheEntry(url.c_str());
	IStream* stream;
	std::string ret = "";
	if (SUCCEEDED(URLOpenBlockingStream(NULL, url.c_str(), &stream, 0, NULL))) {
		STATSTG stats;
		stream->Stat(&stats, STATFLAG_NONAME);
		DWORD size = stats.cbSize.LowPart;
		CHAR* chars = new CHAR[size + 1];
		stream->Read(chars, size, NULL);
		chars[size] = '\0';
		ret = std::string(chars);
		delete[] chars;
	}
	stream->Release();
	return ret;
}
void Resources::Download(std::string url, std::function<void(std::string)> callback) {
	thread_jobs.push([this, url, callback]() {
		std::string s = Download(url);
		todo.push([callback, s]() { callback(s); });
	});
}

void Resources::EnsureFileExists(std::string path_to_file, 
	std::string url, std::function<void(bool)> callback) {

	if (PathFileExists(path_to_file.c_str())) {
		// if file exists, run the callback immediately in the same thread
		callback(true);
	} else {
		// otherwise try to download it in the worker
		Download(path_to_file, url, callback);
	}
}

void Resources::LoadTextureAsync(IDirect3DTexture9** texture, 
	std::string path_to_file, std::string url) {

	if (PathFileExists(path_to_file.c_str())) {
		toload.push([path_to_file, texture](IDirect3DDevice9* device) {
			D3DXCreateTextureFromFile(device, path_to_file.c_str(), texture);
		});
	} else {
		Download(path_to_file, url, 
			[this, path_to_file, texture](bool success) {
			if (success) {
				toload.push([path_to_file, texture](IDirect3DDevice9* device) {
					D3DXCreateTextureFromFile(device, path_to_file.c_str(), texture);
				});
			}
		});
	}
}

void Resources::DxUpdate(IDirect3DDevice9* device) {
	while (!toload.empty()) {
		toload.front()(device);
		toload.pop();
	}
}

void Resources::Update() {
	while (!todo.empty()) {
		todo.front()();
		todo.pop();
	}
}
