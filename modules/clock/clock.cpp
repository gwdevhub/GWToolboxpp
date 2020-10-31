#include "clock.h"

DLLAPI TBModule* getObj() {
	return new Clock();
}

DLLAPI const char* getName() {
	return "Clock";
}

const char* Clock::Name() const {
	return getName();
}

void Clock::Draw(IDirect3DDevice9* device) {
	ImGui::Begin("clock test");
	ImGui::Text("hello, I am a plugin!");
	ImGui::End();
}

