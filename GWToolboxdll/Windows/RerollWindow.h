#pragma once

#include "ToolboxWindow.h"

namespace GW::Constants {
    enum class MapID : uint32_t;
    enum class Profession : uint32_t;
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

    struct Settings {
        bool travel_to_same_location_after_rerolling = true;
        bool rejoin_party_after_rerolling = true;
        bool return_on_fail = false;
    };

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    bool Reroll(const wchar_t* character_name, bool same_map = true, bool same_party = true, const bool ignore_current_character = false, const bool do_not_prompt = false);
    bool Reroll(const wchar_t* character_name, GW::Constants::MapID _map_id);

    void DrawSettingsInternal() override;

    // Find the best available character for the given profession (checks preferred
    // characters first, then falls back to the first unlisted match) and initiate a reroll.
    static bool RerollToProfession(GW::Constants::Profession profession, bool same_map = true, bool same_party = true);

    // Returns the name of the available character that would be used by RerollToProfession,
    // or nullptr if no character with the given profession exists.
    [[nodiscard]] static const wchar_t* FindAvailableCharForProfession(GW::Constants::Profession profession);

    // Returns true while a reroll is in progress (i.e. between the Reroll() call and
    // the character being fully loaded into the target map).
    [[nodiscard]] static bool IsRerolling();
};
