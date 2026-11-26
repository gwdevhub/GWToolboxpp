#pragma once

#include <ToolboxWindow.h>

class GuildWarsPreferencesWindow : public ToolboxWindow {
    GuildWarsPreferencesWindow() = default;
    ~GuildWarsPreferencesWindow() override = default;

public:
    static GuildWarsPreferencesWindow& Instance()
    {
        static GuildWarsPreferencesWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Guild Wars Preferences"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COGS; }

    void Initialize() override;

    void Draw(IDirect3DDevice9* pDevice) override;

private:
};
