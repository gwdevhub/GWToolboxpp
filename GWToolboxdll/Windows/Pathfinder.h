#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxWindow.h>


class Pathfinder : public ToolboxWindow {
    Pathfinder() = default;
    ~Pathfinder() override = default;

public:
    static Pathfinder& Instance()
    {
        static Pathfinder instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pathfinder"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP; }

    void Initialize() override;

    void Draw(IDirect3DDevice9* pDevice) override;
};
