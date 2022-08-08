#pragma once
#include <ToolboxModule.h>
#include <ToolboxUIElement.h>
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
    DialogModule() {};
    ~DialogModule() {};
public:
    static DialogModule& Instance() {
        static DialogModule instance;
        return instance;
    }

    const char* Name() const override { return "Dialogs"; }

    void Initialize() override;
    void Terminate() override;
    const wchar_t* GetDialogBody();
    uint32_t GetDialogAgent();
    const std::vector<GW::UI::DialogButtonInfo*>& GetDialogButtons();
    const std::vector<GuiUtils::EncString*>& DialogModule::GetDialogButtonMessages();
};
