#pragma once

#include <GWCA/GameEntities/Frame.h>

#include <Timer.h>

// A widget we can attach to a native GW frame via AddGuildWarsUI_Frame.
//
// Deliberately does NOT inherit from GW::UI::Frame. GWCA's own Frame subclasses (ButtonFrame,
// CheckboxFrame, TabsFrame, ...) only ever add methods, never data members, precisely because an
// instance is a pointer-cast view onto memory the *game* allocated - sizeof(CheckboxFrame) ==
// sizeof(UI::Frame), so the cast is free. Adding real data members (width, height, ...) on top of
// that would read/write past the end of memory the game actually owns the moment anything treats
// a real in-game frame pointer as one of these. This is a plain, independently heap-allocated
// object instead: `frame` below points at the real native frame once AddGuildWarsUI_Frame creates
// it, but the object itself is fully ours.
class GuildWarsUI_Frame {
public:
    virtual ~GuildWarsUI_Frame() = default;

    // Set once the real native frame exists; null before then.
    GW::UI::Frame* frame = nullptr;

    // Creates the underlying native frame as a child of parent_frame_id and returns it. Use the
    // most specific GWCA factory available for the widget type (e.g. GW::CheckboxFrame::Create)
    // rather than a generic CreateUIComponent call - GWCA's factories already resolve the correct
    // callback/flags internally, so there's nothing to rebuild by hand.
    virtual GW::UI::Frame* CreateNativeFrame(uint32_t parent_frame_id) = 0;

    // Called when the user clicks this widget's frame (see GuildWarsUI_Tab). No-op by default.
    virtual void OnClick() {}

    // Width/height of the widget. Safe to call before the native frame exists (e.g. right after
    // construction, before AddGuildWarsUI_Frame/kInitFrame has run) - the value is just stored and
    // picked up whenever ApplyLayout next runs (AddGuildWarsUI_Frame calls it as part of creation).
    // If the frame already exists, the change is applied immediately instead.
    void SetSize(float width, float height);

    // Explicit position.left/top/right/bottom on the underlying native frame. Same deferred-or-
    // immediate rule as SetSize. Once called, these four values win over SetSize's derived
    // right/bottom on the next ApplyLayout - call SetSize first if you want both.
    void SetMargins(float left, float top, float right, float bottom);

    // Pushes the current width/height/margin state onto `frame`. No-op if `frame` is still null.
    // AddGuildWarsUI_Frame calls this right after creating the native frame; SetSize/SetMargins
    // call it themselves when the frame already exists.
    void ApplyLayout();

    // Current effective size. Once `frame` exists this is read back off frame->position rather
    // than width_/height_ directly - SetMargins can set position.right/bottom independently of
    // width_/height_ (explicit_margins_), so the stored fields alone can go stale. Before `frame`
    // exists there's nothing to read back, so it falls back to the stored intent.
    //
    // A layout that stacks multiple widgets (see GuildWarsUI_Tab) must use these - not a hardcoded
    // row height - or its own kMeasureContent total will disagree with what it actually lays out,
    // and the two are read by different sides of the same engine handoff.
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

// The last (most specific) registered callback on a frame. frame_callbacks is a stack of layered
// handlers, not a single slot - GWCA's own GetLastFrameContext deliberately walks it backward
// because the *last* entry is the most specific one (e.g. a decorator layered on top of a base
// handler). Only useful for widget kinds with no dedicated GWCA factory to call directly (see
// GuildWarsUI_Tab, which has to steal ScrollableFrame's own callbacks this way); GuildWarsUI_
// Checkbox above doesn't need it.
GW::UI::UIInteractionCallback GetLastFrameCallback(GW::UI::Frame* frame);

// Creates the native frame for `child` as a child of `parent`, applies whatever size/margins have
// been set on it so far, and stores the result on child->frame. Returns the created frame, or
// nullptr on failure.
GW::UI::Frame* AddGuildWarsUI_Frame(GW::UI::Frame* parent, GuildWarsUI_Frame* child);

// A native Options-style tab whose content is a vertically-stacked list of GuildWarsUI_Frame
// widgets, sized to fit and laid out generically - nothing here depends on what the widgets
// actually are, only on how many there are and how tall each one is. Add widgets with AddChild
// before or after the tab itself exists; Poll() creates the tab lazily once its target TabsFrame
// is available, and throttles itself so it's safe to call on every Update().
//
// Extends GuildWarsUI_Frame like any other widget - `frame` here is the tab's own content item
// (the one that actually receives kInitFrame/kSetLayout/kMeasureContent/...), created via
// CreateNativeFrame like any other widget's, just via GW::TabsFrame::AddTab+AddItem instead of
// CreateUIComponent directly (a tab can't exist without also being registered with its parent
// TabsFrame, so the two steps happen together in CreateNativeFrame rather than in AddGuildWarsUI_
// Frame's caller). SetSize/SetMargins/ApplyLayout are inherited but unused - a tab's item is sized
// by the engine from kMeasureContent's answer, not imposed by us the way a plain child widget's is.
class GuildWarsUI_Tab : public GuildWarsUI_Frame {
public:
    explicit GuildWarsUI_Tab(std::wstring label);
    ~GuildWarsUI_Tab() override;

    // Adds a widget to this tab's content, stacked below any already added. Safe to call before or
    // after the tab itself exists - if it already does, the widget is created and attached right
    // away; otherwise it's picked up the next time the tab (re)creates its content. GuildWarsUI_Tab
    // owns the widget from this point on (deleted in ~GuildWarsUI_Tab).
    void AddChild(GuildWarsUI_Frame* child);

    // Creates the tab under `tabs` if it isn't already active. Safe to call on every Update()
    // regardless of whether `tabs` is available yet or the tab is already active - internally
    // throttled and a no-op once active.
    void Poll(GW::TabsFrame* tabs);

    // Removes the tab if active. Safe to call unconditionally (e.g. on module terminate).
    void Remove();

    // Switches the Options window to this tab. No-op if the tab isn't active yet. Not called
    // automatically on creation (see CreateNativeFrame's comment - GW persists whichever tab was
    // selected and tries to restore it the next time Options opens; if that's ours and we haven't
    // recreated it yet by then, the engine's own tab-button refresh path crashes on a missing
    // page). Caller's responsibility to know that risk before selecting programmatically.
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
    // Set by AddChild whenever the content actually changes; cleared by HandleMessage's kSetLayout
    // case after it redraws. Without this, redrawing on every kSetLayout dispatch triggers an
    // infinite redraw<->relayout loop with the engine - see HandleMessage's kSetLayout comment.
    bool needs_redraw_ = true;

    // Real per-item handler stolen off a throwaway ScrollableFrame (see CreateNativeFrame) - GWCA
    // has no factory for "the default scrollable-list item handler", so this is the only way to get
    // a legitimate function pointer for it. Frame::DispatchToHandlers (the engine's own dispatcher
    // for kSetLayout/kMeasureContent, confirmed via Ghidra RE of Gw.exe) only ever invokes one
    // frame_callbacks entry - not a broadcast - so this can't just be layered on alongside our own
    // callback; HandleMessage calls it explicitly, and only for messages that don't touch its own
    // internal 4-child template (kSetLayout/kMeasureContent own their content fully instead - see
    // HandleMessage - since cascading there dereferences a child it expects but we never create,
    // and asserts on the resulting null pointer).
    GW::UI::UIInteractionCallback default_item_callback_ = nullptr;

    void HandleMessage(GW::UI::InteractionMessage* message, void* wparam, void* lparam);
    static void ItemCallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam);
};
