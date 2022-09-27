#include <module_base.h>

class Clock : public ToolboxPlugin {
public:
    const char* Name() const override { return "Clock"; }

    void Draw(IDirect3DDevice9* device) override;
    void Initialize(ImGuiContext* ctx) override;
    void Terminate() override;
};
