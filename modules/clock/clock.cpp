#include "clock.h"

DLLAPI TBModule* TBModuleInstance() {
	static Clock instance;
	return &instance;
}

void Clock::Draw(IDirect3DDevice9* device) {
	ImGui::Begin("clock test");
	ImGui::Text("hello, I am a plugin!");
	ImGui::End();
}

