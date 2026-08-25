#pragma once

#include <GWCA/GameEntities/Frame.h>

#include <Timer.h>

class GuildWarsUI_Frame {
public:
    virtual ~GuildWarsUI_Frame() = default;

    // Set once the real native frame exists; null before then.
    GW::UI::Frame* frame = nullptr;

    virtual GW::UI::Frame* CreateNativeFrame(uint32_t parent_frame_id) = 0;

    // Called when the user clicks this widget's frame (see GuildWarsUI_Tab). No-op by default.
    virtual void OnClick() {}

    void SetSize(float width, float height);

    void SetMargins(float left, float top, float right, float bottom);

    void ApplyLayout();

    float GetWidth() const;
    float GetHeight() const;

protected:
    float width_ = 220.f;
    float height_ = 24.f;
    float margin_left_ = 0.f;
    float margin_top_ = 0.f;
    float margin_right_ = 0.f;
    float margin_bottom_ = 0.f;
    uint32_t creation_flags = 0;
    // False until SetMargins is called explicitly - until then, right/bottom are derived from
    // width_/height_ (anchored at margin_left_/margin_top_) rather than taken literally.
    bool explicit_margins_ = false;
};

class GuildWarsUI_Button : public GuildWarsUI_Frame {
public:
    GuildWarsUI_Button(std::wstring label, void (*onClick)());

    GW::UI::Frame* CreateNativeFrame(uint32_t parent_frame_id) override;
    void OnClick() override
    {
        if (onClick_) onClick_();
    }

protected:
    std::wstring label_;
    void (*onClick_)() = nullptr;
};

class GuildWarsUI_Checkbox : public GuildWarsUI_Button {
public:
    GuildWarsUI_Checkbox(std::wstring label, void (*onClick)(), bool* variable = nullptr);

    bool IsChecked() const;
    void SetChecked(bool checked) const;
    GW::UI::Frame* CreateNativeFrame(uint32_t parent_frame_id) override;
    bool* Variable() const { return variable_; }
    void OnClick() override
    {
        *variable_ = !*variable_;
        SetChecked(*variable_);
        GuildWarsUI_Button::OnClick();
    }

private:
    bool* variable_;
    bool local_variable = false;
};

GW::UI::UIInteractionCallback GetLastFrameCallback(GW::UI::Frame* frame);

GW::UI::Frame* AddGuildWarsUI_Frame(GW::UI::Frame* parent, GuildWarsUI_Frame* child);

class GuildWarsUI_Tab : public GuildWarsUI_Frame {
public:
    explicit GuildWarsUI_Tab(std::wstring label);
    ~GuildWarsUI_Tab() override;

    void AddChild(GuildWarsUI_Frame* child);

    void Poll(GW::TabsFrame* tabs);

    // Removes the tab if active. Safe to call unconditionally (e.g. on module terminate).
    void Remove();

    void Select();

    bool IsActive() const { return frame != nullptr; }

    // parent_frame_id must be a GW::TabsFrame (cast internally) - see the class comment for why
    // this does both AddTab and AddItem rather than a single CreateUIComponent call.
    GW::UI::Frame* CreateNativeFrame(uint32_t parent_frame_id) override;

private:
    std::wstring label_;
    std::vector<GuildWarsUI_Frame*> children_;
    uint32_t tabs_frame_id_ = 0;
    uint32_t tab_id_ = 0;
    clock_t last_poll_ = 0;
    bool needs_redraw_ = true;

    GW::UI::UIInteractionCallback default_item_callback_ = nullptr;

    void HandleMessage(GW::UI::InteractionMessage* message, void* wparam, void* lparam);
    static void ItemCallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam);
};
