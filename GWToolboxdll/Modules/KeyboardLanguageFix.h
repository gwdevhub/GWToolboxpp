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
    const char* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    void Initialize() override;
};
