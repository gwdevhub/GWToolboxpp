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
    DialogModule() = default;;
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
    static const std::vector<GuiUtils::EncString*>& DialogModule::GetDialogButtonMessages();
};
