#pragma once

#include <ToolboxWidget.h>

class ActiveQuestWidget : public ToolboxWidget {
    ActiveQuestWidget() = default;
    ~ActiveQuestWidget() override = default;

public:
    static ActiveQuestWidget& Instance()
    {
        static ActiveQuestWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Active Quest Info"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BAHAI; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
};
