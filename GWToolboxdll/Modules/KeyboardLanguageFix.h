#pragma once

#include <ToolboxModule.h>

class KeyboardLanguageFix : public ToolboxModule {
public:
    static KeyboardLanguageFix& Instance()
    {
        static KeyboardLanguageFix instance;
        return instance;
    }
    const char* Name() const override { return "Keyboard Layout Fix"; }
    const char* Description() const override { return "Prevents Guild Wars from adding en-US keyboard language to Windows > 8 without your permission"; }
    const char* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    bool HasSettings() override { return false; }

    void Initialize() override;
};
