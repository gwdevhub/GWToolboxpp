#pragma once

#include <Defines.h>

#include <ToolboxWindow.h>

namespace GW {
    namespace Constants {
        enum class QuestID;
    }
}

class DialogsWindow : public ToolboxWindow {
    DialogsWindow() = default;
    ~DialogsWindow() = default;

public:
    static DialogsWindow& Instance() {
        static DialogsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Dialogs"; }
    const char8_t* Icon() const override { return ICON_FA_COMMENT_DOTS; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

private:
    static constexpr uint32_t QuestAcceptDialog(GW::Constants::QuestID quest) {
        return static_cast<int>(quest) << 8 | 0x800001;
    }
    static constexpr uint32_t QuestRewardDialog(GW::Constants::QuestID quest) {
        return static_cast<int>(quest) << 8 | 0x800007;
    }

    static GW::Constants::QuestID IndexToQuestID(int index);
    static uint32_t IndexToDialogID(int index);

    int fav_count = 0;
    std::vector<int> fav_index{};

    bool show_common = true;
    bool show_uwteles = true;
    bool show_favorites = true;
    bool show_custom = true;

    char customdialogbuf[11] = "";
};
