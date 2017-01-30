#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"

class HealthWindow : public ToolboxWindow {
public:
	HealthWindow() {};

	inline static const wchar_t* IniSection() { return L"TargetHealth"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "health"; }

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;
};
