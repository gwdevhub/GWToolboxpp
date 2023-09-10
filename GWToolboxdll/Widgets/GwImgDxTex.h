#pragma once

#include <ToolboxWidget.h>

class GwImg;

class GwImgTexture : public ToolboxWidget {
    GwImgTexture() = default;
    ~GwImgTexture() override = default;

public:
    static GwImgTexture& Instance()
    {
        static GwImgTexture instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "GwImgTexture"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    private:
    void ClearIcons();
    std::vector<GwImg*> icons;
};
