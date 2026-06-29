#pragma once

#include <filesystem>
#include <string>
#include <vector>

// One row of the first-run readiness checklist: a requirement, whether it is currently met, and where to fix it.
struct AvCheck {
    std::wstring label;
    bool satisfied = false;
    std::wstring settings_uri; // Windows Security page the "Open settings" button launches; empty = no button
    std::wstring hint;
};

// Read-only snapshot of the machine's antivirus state relevant to GWToolbox. Nothing here ever modifies an AV setting.
struct AvReadiness {
    bool read_ok = false;        // the state query succeeded; on false the checklist guides rather than asserts
    bool defender_active = false; // Microsoft Defender is the active real-time antivirus
    std::wstring third_party_av;  // active third-party AV's name, if any (its exclusions can't be read, only Defender's)
    std::vector<AvCheck> checks;
};

// Queries Defender/AV state via PowerShell (read-only) and builds the checklist for the launcher and data folders.
AvReadiness GetAvReadiness(const std::filesystem::path& program_dir, const std::filesystem::path& data_dir);
