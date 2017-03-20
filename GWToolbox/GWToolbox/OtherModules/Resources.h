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
	void Draw(IDirect3DDevice9* device) override;

	//bool EnsureFileExists(const char* name, const char* folder = nullptr);

	// folder should not contain a trailing slash
	void LoadTextureAsync(IDirect3DTexture9** tex,
		const char* name, const char* folder = nullptr);

	// Stops the worker thread once it's done with the current jobs.
	void EndLoading();

private:
	std::queue<std::function<void()>> todo;
	std::queue<std::function<void(IDirect3DDevice9*)>> toload;

	bool should_stop = false;
	std::thread worker;
};
