#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"

class DistanceWindow : public ToolboxWindow {
public:
	DistanceWindow() {};

	inline static const wchar_t* IniSection() { return L"Distance"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "distance"; }

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;
};
