#pragma once

#include <ToolboxPlugin.h>

class TargetOverride : public ToolboxPlugin {
public:
    const char* Name() const override { return "Target Override"; }

	void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;

	void Terminate() override;
};

