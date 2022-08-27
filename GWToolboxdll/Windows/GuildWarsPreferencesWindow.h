#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxWindow.h>
#include <ToolboxUIElement.h>

class GuildWarsPreferencesWindow : public ToolboxWindow {
    GuildWarsPreferencesWindow() = default;
    ~GuildWarsPreferencesWindow() = default;
public:
    static GuildWarsPreferencesWindow& Instance() {
        static GuildWarsPreferencesWindow instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars Preferences"; }
    const char8_t* Icon() const override { return ICON_FA_COGS; }

    void Initialize() override;

    void Draw(IDirect3DDevice9* pDevice) override;

private:
};
