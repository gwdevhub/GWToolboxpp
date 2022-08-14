#pragma once
#include <ToolboxModule.h>

namespace GuiUtils {
    class EncString;
}
namespace GW {
    namespace UI {
        struct DialogButtonInfo;
    }
}
struct DialogButton;

class DialogModule : public ToolboxModule {
    DialogModule() = default;
    ~DialogModule() override = default;

public:
    static DialogModule& Instance() {
        static DialogModule instance;
        return instance;
    }

    const char* Name() const override { return "Dialogs"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    static const wchar_t* GetDialogBody();
    static void SendDialog(uint32_t dialog_id);
    static void SendDialogs(std::initializer_list<uint32_t> dialog_ids);
    static uint32_t GetDialogAgent();
    static const std::vector<GW::UI::DialogButtonInfo*>& GetDialogButtons();
    static const std::vector<GuiUtils::EncString*>& GetDialogButtonMessages();
    // Find and take the first available quest from the current dialog. Returns quest_id requested.
    static uint32_t AcceptFirstAvailableQuest();

private:
    static void OnDialogSent(uint32_t dialog_id);
    static bool IsQuest(const uint32_t dialog_id) { return (dialog_id & 0x800000) != 0; }
    static bool GetQuestID(const uint32_t dialog_id) { return (dialog_id ^ 0x800000) >> 8; }
    static bool IsUWTele(const uint32_t dialog_id) {
        switch (dialog_id) {
            case GW::Constants::DialogID::UwTeleLab:
            case GW::Constants::DialogID::UwTeleVale:
            case GW::Constants::DialogID::UwTelePits:
            case GW::Constants::DialogID::UwTelePools:
            case GW::Constants::DialogID::UwTelePlanes:
            case GW::Constants::DialogID::UwTeleWastes:
            case GW::Constants::DialogID::UwTeleMnt: return true;
            default: return false;
        }
    }
};
