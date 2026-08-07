#pragma once

#include <ToolboxModule.h>

class KeyboardLanguageFix : public ToolboxModule {
public:
    static KeyboardLanguageFix& Instance()
    {
        static KeyboardLanguageFix instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "键盘布局调整"; }
    [[nodiscard]] const char* Description() const override { return "防止激战在 Windows8及以上系统上未经您的许可添加 en-US 键盘语言"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    bool HasSettings() override { return false; }

    void Initialize() override;
};
