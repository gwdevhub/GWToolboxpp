#pragma once
#include <IconsFontAwesome5.h>
#include <SimpleIni.h>

#include <ToolboxModule.h>

class QuestModule : public ToolboxModule {
public:
    static QuestModule& Instance()
    {
        static QuestModule instance;
        return instance;
    }

    const char* Name() const override { return "Quest Module"; }
    const char* Icon() const override { return ICON_FA_COMPASS; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIniA*) override;
    void SaveSettings(CSimpleIniA*) override;
};
