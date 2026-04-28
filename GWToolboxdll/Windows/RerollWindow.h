#pragma once

#include "ToolboxWindow.h"
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Guild.h>

namespace GW::Constants {
    enum class MapID : uint32_t;
    enum class ServerRegion;
    enum class Language;
}

class RerollWindow : public ToolboxWindow {
    RerollWindow() = default;

public:
    static RerollWindow& Instance()
    {
        static RerollWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Reroll"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USERS; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    bool Reroll(const wchar_t* character_name, bool same_map = true, bool same_party = true, const bool ignore_current_character = false, const bool do_not_prompt = false);
    bool Reroll(const wchar_t* character_name, GW::Constants::MapID _map_id);
};
