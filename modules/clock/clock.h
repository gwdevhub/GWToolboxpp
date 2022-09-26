#include <module_base.h>

class Clock : public TBModule {
public:
    const char* Name() const override { return "Clock"; }

    void Draw(IDirect3DDevice9* device) override;
    void Initialize(ImGuiContext* ctx) override;
    void Terminate() override;
};
