# GW native UI — reverse-engineered reference & custom-menu cookbook

Goal: understand Guild Wars' in-game UI (the "Frame" system) well enough to build our
**own native menus/windows** — not ImGui overlays, but real GW frames that live in the
game's UI tree, position with the game's layout, and render through the game's controls.

Evidence base: ReVa/Ghidra on `Gw.388118.exe` + live `Gw.exe` (build 389033), GWCA's
existing UI knowledge (`GWCA/Include/GWCA/Managers/UIMgr.h`, `Source/UIMgr.cpp`,
`Source/Frame.cpp`), and working toolbox examples. Confidence is tagged
**[confirmed]** (read from the binary/verified), **[observed]** (seen in real call
sites), **[inferred]**, **[TODO-RE]**.

---

## 1. The model

GW's UI is a **tree of `Frame` objects**. Each frame has:
- a **`frame_id`** (index into the global frame array; how you reference it) — Frame+0xbc.
- **`component_flags`** — selects the widget type/behaviour — Frame+0x190 **[confirmed]**.
- a **context** — a per-frame data struct (button state, list data, …), reached via
  `GW::UI::GetFrameContext(frame)`.
- an **event callback** (`UIInteractionCallback`) — the frame's behaviour + rendering,
  invoked with UI messages.
- **layout** — a `FramePosition` rect (Frame+0xd8) and a `FrameRelation` node linking it
  into the parent/child/sibling tree.

`Frame` is **0x1c8 bytes [confirmed]** (the game allocates/frees it with that size). Key
offsets (from GWCA `UIMgr.h`, verified against 389033):

| field | offset | notes |
|---|---|---|
| `frame_callbacks` (`Array<FrameInteractionCallback>`) | 0x84 / 0xa8 | the registered callbacks; `CreateUIComponent` inits the array at 0x84, cap 0x40 |
| `child_offset_id` | 0xb8 | this frame's index within its parent |
| `frame_id` | 0xbc | index in the global frame array (return value of create) |
| `field40_0xc0` | 0xc0 | dirty flag; `& 0x100` ⇒ needs redraw |
| redraw list node | 0xd0 | `TriggerFrameRedraw` links `&frame->0xd0` |
| `position` (`FramePosition`, 0x44 bytes) | 0xd8 | logical rect + derived screen-space |
| `relation` (`FrameRelation`) | 0x128 | parent/children/siblings tree |
| `component_flags` | 0x190 | set from `CreateUIComponent`'s 2nd arg |
| `frame_state` | 0x18c | `&0x4`=created, `&0x200`=hidden, `&0x10`=disabled |

`FrameRelation` (0x28) = `{ parent, child_offset_id, parent_dupe, frame_hash_id,
TList children, TList siblings }` — the layout hierarchy.

`InteractionMessage` = `{ uint32_t frame_id; UIMessage message_id; void** wParam; }` —
what the callback receives.

---

## 2. The creation primitive — `CreateUIComponent`

Game function (389033 `0x006308b0`; asserts `P:\Code\Engine\Frame\FrApi.cpp:0x132`).
GWCA wraps it as:

```cpp
uint32_t GW::UI::CreateUIComponent(
    uint32_t parent_frame_id,      // 0 = create under Root; else attach under this frame
    uint32_t component_flags,      // widget type/behaviour bitfield (see §3)
    uint32_t child_index,          // this child's child_offset_id within the parent
    UIInteractionCallback callback,// the frame's behaviour/render handler
    void*    wparam,               // opaque context pointer passed to the callback
    const wchar_t* label);         // frame label (later found via GetFrameByLabel)
// returns the new frame_id.
```

Internally **[confirmed]** it: allocates the 0x1c8 `Frame`, zeroes the callback array
(cap 0x40) at +0x84, stores `component_flags` at +0x190, registers `(callback, wparam)`
via the frame-callback array, links it under the parent by `child_index`, registers the
`label`, then sends init/layout messages (internal msgs 4/2/10) so the frame comes up.

### Real call sites decoded [observed] — the templates to copy
From the game's own UI construction (2011 call sites; representative):

```
CreateUIComponent(0,          0,        0,   RootCb, ctx, L"Root");     // top-level root
CreateUIComponent(parent,     0,        2,   GameCb, 0,   L"Game");     // plain container
CreateUIComponent(parent,     0x40000,  4,   BtnCb,  0,   L"BtnExit");  // button
CreateUIComponent(parent,     0x2040000,5,   BtnCb,  0,   L"BtnRestore");// button +0x2000000
CreateUIComponent(parent,     0x1040000,6,   BtnCb,  0,   L"BtnMin");   // button +0x1000000
CreateUIComponent(parent,     0x2b000,  9,   Cb,     0,   0);           // (unlabeled control)
```

Takeaways:
- **`parent_frame_id = 0` makes a top-level frame under Root.** Otherwise it nests.
- **`child_index` is the `child_offset_id`** you later use with `GetChildFrame(parent, index)`.
  The game frequently uses `0x10000000 + i` for indexed/dynamic children (see §6).
- The **callback is the widget's brain** — reuse a stock control callback (§4) for
  standard widgets, or supply your own for custom behaviour.

---

## 3. `component_flags` — widget type/behaviour [partially decoded]

Observed data points (2nd arg of `CreateUIComponent`):

| flags | used for |
|---|---|
| `0x0` | plain container / grouping frame (Root, "Game") |
| `0x40000` | **interactive button base** (BtnExit) |
| `0x40000 \| 0x1000000` (`0x1040000`) | button variant (BtnMin) |
| `0x40000 \| 0x2000000` (`0x2040000`) | button variant (BtnRestore) |
| `0x400`, `0x2b000`, … | other control styles |

`0x40000` reads as "accepts mouse/click" (button-like); the high bits (`0x1000000`,
`0x2000000`) are per-widget style/behaviour modifiers. **[TODO-RE]** full bitfield: decode
by reading how the frame message dispatcher / control callbacks branch on
`frame->component_flags` (Frame+0x190) — a bounded next step (decompile the button and
text-label callbacks and the layout code that tests these bits).

---

## 4. The callback / message protocol

A frame's `UIInteractionCallback(InteractionMessage* msg, void* wParam, void* lParam)` is
invoked with `msg->message_id`. The vocabulary a menu cares about **[confirmed ids]**:

| message | id | meaning / what to do |
|---|---|---|
| `kInitFrame` | 0x9 | frame created — allocate/attach your context |
| `kDestroyFrame` | 0xb | frame going away — free your context |
| `kMouseClick` | 0x24 | click; wparam = `UIPacket::kMouseClick*` (carries `child_offset_id`) |
| `kMouseClick2`/`kMouseAction` | 0x31/0x32 | mouse press/move/release; wparam = `kMouseAction*` |
| `kRenderFrame_*` | 0x33/0x35/0x4b | paint — draw the frame's content |
| `kSetLayout` | 0x37 | (re)position/size children — do your layout here |
| `kUIPositionChanged` | 0x10000143 | frame moved/resized |

Two ways to give a frame behaviour:
1. **Reuse a stock control callback** (easy path). GWCA already locates them in
   `Frame.cpp`: `Button_UICallback` (assert `UiCtlBtn.cpp/!s_btnCheckImageList`),
   `TextLabel_UICallback` (`CtlText.cpp`), `ScrollableFrameList_UICallback`
   (`CtlFrameList.cpp`), `ScrollableFrame_UICallback`, plus `Default_UICallback`
   (`FrMsg.cpp`). Pass one of these as `CreateUIComponent`'s callback and the game
   renders/behaves as that control type — you just set text/state. **[observed]**
2. **Supply your own callback** and handle the messages above — required only for
   fully-custom rendering (handle `kRenderFrame`) or custom hit-logic.

For a click, the game delivers `kMouseClick` whose packet carries the clicked child's
`child_offset_id` — so a container callback dispatches by `child_offset_id` (exactly how
`ExtraWeaponSets` routes weapon-set clicks, §6).

### 4a. Confirmed control mechanics (from decompiling the button callbacks) [confirmed]
A control callback is `UICtlCallback(frame_obj, msg, lparam)` that `switch`es on a
**control-message opcode** (`msg->…`, the low-level control op):
- **op 5 = init**: load the control's resources (the button callback creates its image
  atlas: `img = LoadUIImage(…)`).
- **op 6 = destroy**: free those resources.
- **op 1 = render/paint**: draw the control. The button callback computes an image index
  from `pressed/hover state` **and** `FrameTestStyles(frame, mask)` — i.e.
  `frame->component_flags (Frame+0x190) & mask` — then blits the button image. So the
  **control callback does the drawing**, and `component_flags` selects the visual style
  (the `0x1000000`/`0x2000000` bits pick button variants). `FrameTestStyles` is the game's
  `frame->component_flags & mask` reader (389033 `FUN_00634710`).
- **unhandled ops fall through to the default control handler** (`Default_UICallback` /
  the game's default), which does standard behaviour.

Implication for us: to get a **standard-looking widget for free**, create it with the
right `component_flags` and pass the game's **stock control callback** (GWCA already
locates `Button_UICallback`, `TextLabel_UICallback`, `ScrollableFrameList_UICallback`) —
it will init its own resources and paint itself. Your **container** frame's callback only
needs to handle `kMouseClick` (dispatch by `child_offset_id`) and `kSetLayout` (position
children). No custom paint required for standard widgets.

---

## 5. Navigation & manipulation API (already in GWCA)

- `GetRootFrame()` / `GetFrameByLabel(L"…")` / `GetFrameById(id)` — find frames.
- `GetChildFrame(parent, child_offset[, …])` — descend the tree (variadic path).
- `GetParentFrame(frame)`.
- `GetFrameContext(frame)` — the frame's data struct.
- `SendFrameUIMessage(frame, message_id, packet)` / `SendUIMessage(...)` — drive it.
- `SetFrameTitle(frame, L"…")` — set label/caption text.
- `ButtonClick(frame)` — programmatic click.
- `SetFramePosition(frame, pos)` (logical rect only) / `TriggerFrameRedraw(frame)` —
  move + repaint. **Do NOT full-copy `FramePosition`** (that clobbers game-derived
  `screen_*`/`viewport_*` — the hero-panel bug, see `docs/gw-auto-update/08`).
- `DestroyUIComponent(frame)` — remove.

---

## 6. Worked example — extending a game frame (`Unused/ExtraWeaponSets.cpp`)

Adds extra weapon-set slots to the native weapon bar:
- children addressed as `GetChildFrame(weaponbar, 0x10000000 + i)`;
- per-slot context `WeaponSetContext` (0x2c) / bar context `WeaponBarContext` (0xc) via
  `GetFrameContext`;
- hooks the bar's callback, on `kMouseClick` reads `packet->child_offset_id - 0x10000000`
  to know which slot was clicked, then `SendFrameUIMessage(..., kWeaponSetSwapComplete)`;
- on `kSetLayout` repositions the extra slots relative to the mirrored native ones.

This is the canonical pattern for **augmenting** existing GW UI.

---

## 7. Cookbook — creating a custom menu

### Approach A — a standalone window under Root (most menu-like)
```cpp
// 1. Create a container frame under Root.
uint32_t root = GW::UI::GetFrameById(0 /*or GetRootFrame()->frame_id*/);
uint32_t menu = GW::UI::CreateUIComponent(
    root, 0x0 /*container*/, MY_MENU_IDX, MyMenuCallback, &my_ctx, L"TBCustomMenu");

// 2. Add child widgets (reuse stock control callbacks for easy rendering).
GW::UI::CreateUIComponent(menu, 0x40000 /*button*/, 0x10000000+0, Button_UICallback, &btn0, L"TBBtn0");
GW::UI::CreateUIComponent(menu, 0x0      /*text*/,  0x10000000+1, TextLabel_UICallback, &lbl0, L"TBLbl0");

// 3. Position: in MyMenuCallback handle kSetLayout — set each child's logical rect via
//    GW::UI::SetFramePosition(child, rect) (NOT a full FramePosition copy).

// 4. Handle clicks: in MyMenuCallback handle kMouseClick — switch on
//    ((UIPacket::kMouseClick*)wParam)->child_offset_id - 0x10000000.

// 5. Set labels/text with GW::UI::SetFrameTitle(child, L"…").
```

### Approach B — dock into an existing menu
Find a host frame (`GetFrameByLabel(L"Options")`, the party window, etc.), create your
component as its child (`CreateUIComponent(hostId, …)`), and lay out relative to its
existing children. This is how the toolbox already adds native options
(`GuildWarsSettingsModule`).

### What still needs runtime + a human eye
Creating frames is safe to script, but **rendering/positioning must be eyeballed** and
must run on the game thread (`GW::GameThread::Enqueue`). Fully-custom drawing (handling
`kRenderFrame` yourself) is the remaining unknown (§8).

---

## 8. Proposed helper API (to make menus *easy*)

A thin toolbox wrapper over the primitives — sketch:

```cpp
namespace NativeUI {
  struct Menu {                 // a container frame under Root or a host
    uint32_t frame_id;
    static Menu Create(const wchar_t* label, uint32_t parent = 0);
    uint32_t AddButton(const wchar_t* label, std::function<void()> on_click);
    uint32_t AddLabel (const wchar_t* label);
    void      SetRect (uint32_t child, float l, float t, float r, float b);
    void      Show(bool);  void Destroy();
  };
}
// Impl wraps CreateUIComponent + the stock control callbacks (Button_UICallback, …),
// keeps a child_offset_id -> std::function map, and a single container callback that
// on kMouseClick dispatches to the map and on kSetLayout applies the stored rects via
// SetFramePosition. All calls marshalled onto the game thread.
```

This turns "make a menu" into `Menu::Create` + `AddButton`/`AddLabel` + `SetRect`.

---

## 9. Next RE steps (bounded)
1. **Decode `component_flags`** fully: decompile `Button_UICallback`/`TextLabel_UICallback`
   and the layout dispatcher, branching on `frame->component_flags` (Frame+0x190).
2. **Custom paint**: RE how a control handles `kRenderFrame_*` (0x33/0x35/0x4b) to draw
   text/quads, so we can render arbitrary content in a custom frame.
3. **Stock control contexts**: the context struct each control callback expects (so we can
   populate a button/label/list correctly) — `GetFrameContext` layout per control.
4. Build the `NativeUI::Menu` helper (above) and eye-test in-game (needs a human/runtime).

> These names/offsets are for build ~388118–389033. The `Frame` layout and the UI message
> ids were verified unchanged in 389033; re-verify after a client update using the
> `tools/gw-update/` scanners.
