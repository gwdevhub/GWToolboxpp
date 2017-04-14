#pragma once

#include <queue>
#include <functional>
#include <thread>
#include <d3d9.h>

#include "ToolboxModule.h"

class Resources : public ToolboxModule {
	Resources() {};
	~Resources() {
		if (worker.joinable()) worker.join();
	};
public:
	static Resources& Instance() {
		static Resources instance;
		return instance;
	}

	const char* Name() const override { return "Resources"; }

	void Initialize() override;
	void Terminate() override;

	void DrawSettings() override {};
	void DxUpdate(IDirect3DDevice9* device);

	// folder should not contain a trailing slash
	void LoadTextureAsync(IDirect3DTexture9** tex,
		const char* name, const char* folder, const char* url);

	// checks if file exists, and downloads from server if it doesn't.
	// If the file exists, executes callback immediately,
	// otherwise execute callback on download completion.
	// folder should not contain a trailing slash
	void EnsureFileExists(const char* name, 
		const char* folder, const char* url, 
		std::function<void(bool)> callback = [](bool success) {});

	void EnsureSubPathExists(const char* path) const;
	void EnsureFullPathExists(const char* path) const;

	// Stops the worker thread once it's done with the current jobs.
	void EndLoading();

private:
	bool GetPath(CHAR* path, const char* folder = nullptr) const;

	std::queue<std::function<void()>> todo;
	std::queue<std::function<void(IDirect3DDevice9*)>> toload;

	bool should_stop = false;
	std::thread worker;
};
