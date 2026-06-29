#pragma once

#include <atomic>
#include <filesystem>

#include "AvReadiness.h"
#include "Window.h"

// First-run setup window: shows the read-only AV checklist, lets the user open the relevant Windows Security page per
// item, and re-checks live (on focus, on a timer, and on demand). It never changes an AV setting itself.
class AvReadinessWindow : public Window {
public:
    AvReadinessWindow() = default;
    AvReadinessWindow(const AvReadinessWindow&) = delete;
    ~AvReadinessWindow() override = default;
    AvReadinessWindow& operator=(const AvReadinessWindow&) = delete;

    bool Create() override;

    // Builds the checklist for the launcher's own folder + the Documents data folder and pumps until the user proceeds.
    static void Run();

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    void OnCreate(HWND hWnd);
    void StartCheck();     // kicks a background readiness query unless one is already running
    void ApplyReadiness(); // rebuilds the row controls + status from m_readiness

    std::filesystem::path m_program_dir;
    std::filesystem::path m_data_dir;
    AvReadiness m_readiness;
    std::vector<HWND> m_row_labels;  // one status label per check, coloured by its satisfied state
    std::vector<HWND> m_row_buttons; // "Open Windows Security" per unmet check (nullptr when met / no link)
    std::vector<bool> m_row_satisfied;
    HWND m_hHeader = nullptr;
    HWND m_hStatus = nullptr;
    HWND m_hRecheck = nullptr;
    HWND m_hContinue = nullptr;
    std::atomic<bool> m_checking = false;
};
