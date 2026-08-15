#include "stdafx.h"

#include <unordered_map>

#include <GWCA/Utilities/Scanner.h>

#include "GuildWarsUIFrame.h"

GW::UI::UIInteractionCallback GetLastFrameCallback(GW::UI::Frame* frame)
{
    if (!frame) return nullptr;
    for (size_t i = frame->frame_callbacks.size(); i-- > 0;) {
        if (frame->frame_callbacks[i].callback)
            return frame->frame_callbacks[i].callback;
    }
    return nullptr;
}

namespace {
    // First child_offset_id not already in use under parent. CreateUIComponent takes an explicit
    // id and won't pick one for us - this mirrors GWCA's own (private, unexported) CreateFrame.
    uint32_t FindFreeChildSlot(uint32_t parent_frame_id)
    {
        const auto parent = GW::UI::GetFrameById(parent_frame_id);
        uint32_t child_offset_id = 0xff;
        while (GW::UI::GetChildFrame(parent, child_offset_id)) {
            child_offset_id++;
        }
        return child_offset_id;
    }
}

GW::UI::Frame* AddGuildWarsUI_Frame(GW::UI::Frame* parent, GuildWarsUI_Frame* child)
{
    if (!(parent && child)) return nullptr;
    if (child->frame) return child->frame;
    child->frame = child->CreateNativeFrame(parent->frame_id);
    child->ApplyLayout();
    return child->frame;
}

void GuildWarsUI_Frame::SetSize(float width, float height)
{
    width_ = width;
    height_ = height;
    ApplyLayout();
}

void GuildWarsUI_Frame::SetMargins(float left, float top, float right, float bottom)
{
    margin_left_ = left;
    margin_top_ = top;
    margin_right_ = right;
    margin_bottom_ = bottom;
    explicit_margins_ = true;
    ApplyLayout();
}

float GuildWarsUI_Frame::GetWidth() const
{
    return frame ? (frame->position.right - frame->position.left) : width_;
}

float GuildWarsUI_Frame::GetHeight() const
{
    return frame ? (frame->position.top - frame->position.bottom) : height_;
}

void GuildWarsUI_Frame::ApplyLayout()
{
    if (!frame) return; // Nothing to push onto yet - SetSize/SetMargins already saved the state,
                         // and whichever call eventually creates `frame` applies it then.
    frame->position.left = margin_left_;
    frame->position.top = margin_top_;
    frame->position.right = explicit_margins_ ? margin_right_ : margin_left_ + width_;
    frame->position.bottom = explicit_margins_ ? margin_bottom_ : margin_top_ - height_;
    //GW::UI::TriggerFrameRedraw(frame);
}

GuildWarsUI_Checkbox::GuildWarsUI_Checkbox(std::wstring label, void (*onClick)(), bool* variable) : GuildWarsUI_Button(std::move(label), onClick), variable_(variable ? variable : &local_variable)
{
    creation_flags = 0x80000;
}
GuildWarsUI_Button::GuildWarsUI_Button(std::wstring label, void (*onClick)())
    : label_(std::move(label)), onClick_(onClick)
{
    creation_flags = 0x40000;
    height_ = 24.f;
}

bool GuildWarsUI_Checkbox::IsChecked() const
{
    return *variable_;
}

void GuildWarsUI_Checkbox::SetChecked(bool checked) const
{
    if (frame) ((GW::CheckboxFrame*)frame)->SetChecked(checked);
}

namespace {
    // GWCA's own CheckboxFrame::Create (Button_UICallback, "UiCtlBtn.cpp"/"!s_btnCheckImageList")
    // isn't used here - resolved independently via the same GW::Scanner API GWCA itself is built
    // on, so this doesn't depend on GWCA's (unverified - CheckboxFrame::Create has zero other
    // callers anywhere) flags convention at all.
    GW::UI::UIInteractionCallback ResolveButtonCallback()
    {
        static GW::UI::UIInteractionCallback cached = nullptr;
        if (cached) return cached;
        const auto site = GW::Scanner::FindAssertion("UiCtlBtn.cpp", "!s_btnCheckImageList", 0, 0);
        cached = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(site);
        return cached;
    }
}

GW::UI::Frame* GuildWarsUI_Checkbox::CreateNativeFrame(uint32_t parent_frame_id)
{
    GuildWarsUI_Button::CreateNativeFrame(parent_frame_id);
    SetChecked(*variable_);
    return frame;
}

GW::UI::Frame* GuildWarsUI_Button::CreateNativeFrame(uint32_t parent_frame_id)
{
    const auto callback = ResolveButtonCallback();
    if (!callback) return nullptr;

    const auto encoded = std::format(L"\x108\x107{}\x1", label_);
    const auto child_offset_id = FindFreeChildSlot(parent_frame_id);
    const auto frame_id = GW::UI::CreateUIComponent(parent_frame_id, creation_flags, child_offset_id, callback, (void*)encoded.c_str(), label_.c_str());
    return frame = (frame_id ? GW::UI::GetFrameById(frame_id) : nullptr);
}

namespace {
    constexpr clock_t kPollMs = 500;
    constexpr float kTabContentWidth = 220.f;

    std::unordered_map<uint32_t, GuildWarsUI_Tab*> g_tabs_by_item_frame_id;

    // Set to the tab under construction for the duration of its AddItem call below, nullptr
    // otherwise. Needed because kInitFrame can fire *synchronously from inside* AddItem (confirmed
    // live - see tabdiag logging), before AddItem has returned the frame_id CreateNativeFrame needs
    // to register g_tabs_by_item_frame_id. Without this fallback, that first kInitFrame's frame_id
    // isn't in the map yet, ItemCallback's lookup misses, and the message is silently dropped -
    // since kInitFrame only fires once (lazy content creation), the tab's children never get
    // created and the tab shows permanently blank.
    GuildWarsUI_Tab* g_tab_under_construction = nullptr;
}

GuildWarsUI_Tab::GuildWarsUI_Tab(std::wstring label)
    : label_(std::move(label))
{
}

GuildWarsUI_Tab::~GuildWarsUI_Tab()
{
    for (auto& child : children_) {
        delete child;
    }
}

void GuildWarsUI_Tab::AddChild(GuildWarsUI_Frame* child)
{
    children_.push_back(child);
    needs_redraw_ = true;
    AddGuildWarsUI_Frame(frame, child);
}

void GuildWarsUI_Tab::Poll(GW::TabsFrame* tabs)
{
    if (frame) return;
    if (TIMER_DIFF(last_poll_) < kPollMs) return;
    last_poll_ = TIMER_INIT();
    if (!tabs) return;
    frame = CreateNativeFrame(tabs->frame_id);
    // Deliberately not calling ApplyLayout() - see the class comment: a tab's item is sized by the
    // engine from kMeasureContent's answer, not imposed via SetSize/margins like a plain child's.
}

GW::UI::Frame* GuildWarsUI_Tab::CreateNativeFrame(uint32_t parent_frame_id)
{
    const auto tabs = (GW::TabsFrame*)GW::UI::GetFrameById(parent_frame_id);
    if (!tabs) return nullptr;

    // Create a temporary scrollable frame purely to steal the real callbacks the engine wires up
    // for one - GWCA has no factory for "the default scrollable-list item handler", so this is the
    // only way to get a legitimate function pointer for it.
    const auto tmp_frame = GW::ScrollableFrame::Create(tabs->frame_id);
    const auto scrollable_callback = GetLastFrameCallback(tmp_frame);
    const auto inner_callback = GetLastFrameCallback(tmp_frame->GetPage());
    GW::UI::DestroyUIComponent(tmp_frame);
    default_item_callback_ = inner_callback;

    const auto encoded_label = std::format(L"\x108\x107{}\x1", label_);
    GW::ScrollableFrame::ScrollablePageContext packet = {.flags = 0, .page_callback = inner_callback, .wparam = 0};
    // 0xff (not a guessed slot number): passing an explicit small child_offset_id here crashes
    // (confirmed via crash dump + Ghidra - the engine's own tab-button refresh path double-
    // complements a caller-picked id and trips CtlPage.cpp's "!IsBtnCode" assert). Read the id the
    // engine actually assigned back off the returned frame.
    const auto scrollable_tab = (GW::ScrollableFrame*)tabs->AddTab(encoded_label.c_str(), 0x20000, 0xff, scrollable_callback, &packet);
    if (!scrollable_tab) return nullptr;
    tab_id_ = scrollable_tab->child_offset_id;
    tabs_frame_id_ = tabs->frame_id;

    g_tab_under_construction = this;
    const auto item_frame_id = scrollable_tab->AddItem(0, 0, ItemCallback);
    g_tab_under_construction = nullptr;
    if (!item_frame_id) return nullptr;
    g_tabs_by_item_frame_id[item_frame_id] = this;

    // NOT auto-selecting the tab here (a previous version called tabs->ChooseTab(tab_id_)) -
    // confirmed via crash dump (P:\Code\Engine\Controls\CtlPage.cpp(558), assertion "btnFrame")
    // that GW remembers whichever tab was selected and tries to restore that selection the next
    // time the Options window opens; if that remembered tab was ours and got destroyed when the
    // window last closed, the engine's own tab-button refresh path dereferences a stale/null
    // button frame and crashes. A user manually clicking our tab hits the same "remembered tab"
    // mechanism, but that's an existing, pre-existing risk in GW itself, not something introduced
    // by auto-selecting - only the auto-select path is ours to avoid triggering unnecessarily.
    GW::UI::TriggerFrameRedraw(tabs);
    return GW::UI::GetFrameById(item_frame_id);
}

void GuildWarsUI_Tab::Select()
{
    if (!frame) return;
    const auto tabs = (GW::TabsFrame*)GW::UI::GetFrameById(tabs_frame_id_);
    if (tabs) tabs->ChooseTab(tab_id_);
}

void GuildWarsUI_Tab::Remove()
{
    if (!frame) return;
    const auto tabs = (GW::TabsFrame*)GW::UI::GetFrameById(tabs_frame_id_);
    if (tabs) tabs->RemoveTab(tab_id_);
}

void GuildWarsUI_Tab::ItemCallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
{
    const auto found = g_tabs_by_item_frame_id.find(message->frame_id);
    if (found != g_tabs_by_item_frame_id.end()) {
        found->second->HandleMessage(message, wparam, lparam);
        return;
    }
    // First dispatch arrived synchronously from inside AddItem, before CreateNativeFrame could
    // register this frame_id - see g_tab_under_construction's comment. Register it now and handle.
    if (g_tab_under_construction) {
        g_tabs_by_item_frame_id[message->frame_id] = g_tab_under_construction;
        g_tab_under_construction->HandleMessage(message, wparam, lparam);
    }
}

void GuildWarsUI_Tab::HandleMessage(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
{
    // Resolved from message->frame_id, not the inherited `frame` member - kInitFrame/kSetLayout can
    // fire synchronously from *inside* AddItem, before CreateNativeFrame has returned and its
    // caller (Poll()) has had a chance to assign the result to `frame`. Reading `frame` here at
    // that point is a null-pointer footgun (frame->position.top on a still-null frame); message->
    // frame_id is always correct regardless of how far our own bookkeeping has caught up.
    const auto item = GW::UI::GetFrameById(message->frame_id);
    switch (message->message_id) {
        case GW::UI::UIMessage::kInitFrame: {
            // Guard against re-creating a child whose native frame already exists - confirmed live
            // (StackWalk64 against a hung Gw.exe, see tools/live_thread_peek.py) that without this,
            // a re-fired kInitFrame on an already-initialized item creates another native frame for
            // every child on every firing, unbounded. Each new frame also makes FindFreeChildSlot's
            // linear scan for a free child_offset_id progressively more expensive, so this doesn't
            // fail fast - it degrades into what looks like a dead freeze rather than a crash.
            bool created_any = false;
            for (auto& child : children_) {
                if (child->frame) continue;
                created_any |= AddGuildWarsUI_Frame(item, child) != nullptr;
            }
            if (created_any) {
                // A child created here needs a kSetLayout pass to actually get positioned/drawn - the
                // engine doesn't reliably re-run kSetLayout on its own just because kInitFrame lazily
                // created new content, so drive it explicitly. needs_redraw_ re-arms the redraw for
                // that one resulting kSetLayout pass; TriggerFrameRedraw is what actually asks the
                // engine to run it. Safe from the earlier infinite-loop bug because kSetLayout's own
                // redraw is gated on needs_redraw_ too, so this can only cause one extra pass, not a
                // cascade - see the kSetLayout case below.
                needs_redraw_ = true;
                GW::UI::TriggerFrameRedraw(item);
            }
        } return;
        case GW::UI::UIMessage::kSetLayout: {
            // Generic: stack children_ top-to-bottom using each one's own height, through the real
            // engine's own Frame::SetPositionInternal (via GWCA's SetFramePosition) rather than
            // writing frame->position fields by hand. StretchWidth|StretchHeight resolve a child's
            // rect to exactly the rect passed in (see FrameLayoutMode's doc comment in UIMgr.h).
            //
            // GW::UI::TriggerFrameRedraw(item) below is needed for the tab's children to actually
            // paint (confirmed live: without it the checkbox exists but never renders). But calling
            // it unconditionally re-enters the engine's redraw path, which re-dispatches kSetLayout
            // right back into this same handler via ItemCallback - confirmed live via StackWalk64
            // against a hung Gw.exe (repeated OnSendEventMessage -> SendFrameUIMessage -> ItemCallback
            // -> kSetLayout cycles, CPU pegged, addresses genuinely advancing each sample so it's a
            // real sustained loop, not a lock wait). A same-call reentrancy guard does NOT fix this:
            // each dispatch is a fresh top-level call from the engine (stack unwinds between them),
            // not synchronous recursion within one call. Fix is a dirty flag instead - only redraw
            // once after something actually changed (AddChild sets it), not on every kSetLayout the
            // engine chooses to refire.
            const auto row_mode = GW::UI::FrameLayoutMode_AnchorLeft | GW::UI::FrameLayoutMode_AnchorTop;
            float top = item->position.top;
            for (auto& child : children_) {
                if (!child->frame) continue;
                const auto height = child->GetHeight();
                // GW::UI::FramePosition member order - left, bottom, right, top - not left/top/right/bottom.
                float rect[4] = {item->position.left, top - height, item->position.right, top};
                GW::UI::SetFramePosition(child->frame, row_mode, rect);
                top -= height;
            }
            if (needs_redraw_) {
                needs_redraw_ = false;
                GW::UI::TriggerFrameRedraw(item);
            }
        } return;
        case GW::UI::UIMessage::kMeasureContent: {
            // Must equal the sum kSetLayout stacks against, above - the engine sizes/positions the
            // item frame from this value before kSetLayout ever runs, so a mismatch here means
            // kSetLayout either stacks into leftover empty space or overflows a container smaller
            // than what it's stacking.
            const auto packet = (GW::UI::UIPacket::kMeasureContent*)wparam;
            if (packet && packet->size_output) {
                float total_height = 0.f;
                for (auto& child : children_) {
                    total_height += child->GetHeight();
                }
                packet->size_output[0] = kTabContentWidth;
                packet->size_output[1] = total_height;
            }
        } return;
        case GW::UI::UIMessage::kMouseClick2: {
            // Generic dispatch only: find which child was clicked and let it handle its own click
            // logic (see GuildWarsUI_Checkbox::OnClick) - the tab doesn't need to know or care what
            // kind of widget it is or what a click means for it.
            const auto packet = (GW::UI::UIPacket::kMouseAction*)wparam;
            if (packet->current_state != GW::UI::UIPacket::ActionState::MouseUp) return;
            const auto clicked = GW::UI::GetFrameById(packet->frame_id);
            for (auto& child : children_) {
                if (child->frame == clicked) {
                    child->OnClick();
                    break;
                }
            }
        } return;
        case GW::UI::UIMessage::kDestroyFrame: {
            g_tabs_by_item_frame_id.erase(message->frame_id);
            frame = nullptr;
            for (auto& child : children_) {
                child->frame = nullptr;
            }
            // Previously forged TabsFrame context/frame_state here to force-deselect our tab
            // before it was destroyed (GW remembers the last-selected tab across reopens, and it
            // can't be ours - we won't have re-added it yet). That code didn't match its own
            // comment and correlated with a reproducible crash on the next Options reopen. Removed
            // rather than guessed at further; worst case now is a stale/blank tab selection on
            // reopen, not a crash.
        } return;
        case GW::UI::UIMessage::kRefreshContent: {
            GW::UI::TriggerFrameRedraw(item);
        } return;
        default:
            break;
    }
    // Only genuinely unhandled messages (e.g. paint setup) reach the real per-item handler - see
    // default_item_callback_'s comment for why kSetLayout/kMeasureContent above deliberately don't.
    if (default_item_callback_) {
        default_item_callback_(message, wparam, lparam);
    } else {
        GW::UI::Default_UICallback(message, wparam, lparam);
    }
}
