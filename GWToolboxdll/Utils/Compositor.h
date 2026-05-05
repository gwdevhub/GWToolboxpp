#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>
#include <d3d9.h>

#include <GWCA/Managers/UIMgr.h>

namespace Compositor {

    struct ScreenRect {
        float left, top, right, bottom;

        bool Overlaps(const ScreenRect& other) const {
            return left < other.right && right > other.left &&
                   top < other.bottom && bottom > other.top;
        }

        bool ContainsPoint(float x, float y) const {
            return x >= left && x <= right && y >= top && y <= bottom;
        }

        bool IsValid() const { return right > left && bottom > top; }
    };

    // Unified z-order tracker for TB (ImGui) and GW windows.
    class UnifiedZOrder {
    public:
        void Update();
        void OnWindowFocused(const char* tb_name);
        uint64_t GetZ(const char* tb_name) const;
        void OnGWWindowClicked(float x, float y);

        struct GWWindowInfo {
            GW::UI::Frame* frame;
            ScreenRect bounds;
            uint64_t unified_z;
        };

        // Merged z-order entry for compositing
        struct CompositeEntry {
            uint64_t z;
            bool is_tb;              // true=TB window, false=GW window
            const char* tb_name;     // if TB
            GW::UI::Frame* gw_frame; // if GW
            ScreenRect bounds;
        };

        // Build the merged z-order list for the current frame.
        const std::vector<CompositeEntry>& GetCompositeOrder() const { return composite_order_; }

        const std::vector<GWWindowInfo>& GetGWWindows() const { return gw_windows_; }

        // Geometric hit-test: find the topmost (highest z) TB window at the given point.
        uint64_t GetTopTBWindowZAtPoint(float x, float y) const;

    private:
        uint64_t next_z_ = 1;
        std::unordered_map<std::string, uint64_t> tb_z_;
        std::unordered_map<GW::UI::Frame*, uint64_t> gw_z_; // persistent z per GW overlay frame
        std::unordered_set<GW::UI::Frame*> current_frames_;  // reused each frame
        std::vector<GWWindowInfo> gw_windows_;
        std::vector<CompositeEntry> composite_order_;
    };

    UnifiedZOrder& GetUnifiedZOrder();

    // Install hooks on GW's render pipeline.
    // Returns true if hooks are active, false if scanning failed (renders TB on top).
    bool HookFrCacheRender();

    // Returns true if the render pipeline hooks are installed and active.
    bool IsHooked();

    // Store D3D device for use during hooks.
    void SetDevice(IDirect3DDevice9* device);

    // Register a callback that runs the ImGui frame cycle (NewFrame through Render).
    // Called from FrCacheRender_Hook before GW's render pass so draw lists are fresh.
    using FrameDrawCallback = void(*)(IDirect3DDevice9*);
    void SetFrameDrawCallback(FrameDrawCallback callback);

    // Release all D3D resources (call on device reset).
    void ReleaseDeviceResources();

    // Register an overlay callback for a specific GW frame (or nullptr for base UI).
    // Lower priority renders first within the same GW frame.
    // Callbacks persist across frames — register once during Initialize.
    using OverlayRenderCallback = std::function<void(IDirect3DDevice9*)>;
    void RegisterOverlayCallback(const wchar_t* gw_frame_label, OverlayRenderCallback callback, int priority = 0);

    // Convenience: register an ImGui-style overlay. The draw function receives a
    // draw list that is automatically clipped to the GW frame bounds and rendered.
    using ImGuiOverlayCallback = std::function<void(ImDrawList&)>;
    void RegisterImGuiOverlay(const wchar_t* gw_frame_label, ImGuiOverlayCallback draw_fn, int priority = 0);

    // Run the prepare phase of ImGui overlay callbacks.  Must be called during the
    // ImGui frame (before EndFrame) so that draw_fns can use ImGui functions like
    // SetTooltip.  The resulting draw lists are cached and rendered later during
    // compositing.
    void PrepareOverlays();

    // Render a named ImGui window's draw list directly to the backbuffer, then clear it
    // so the normal TB compositing skips it. Sets up ImGui D3D state; expects the caller
    // (or the compositing loop) to manage the outer state block.
    void RenderImGuiWindow(IDirect3DDevice9* device, const char* window_name);

    // Check if a screen point is occluded by a GW window that's above the given TB window.
    // Returns true if the point is over a GW floating window with higher z than tb_z.
    bool IsPointOccludedByGW(float x, float y, uint64_t tb_z);

    // Find the z-value of the TB window under the cursor (from ImGui hover/active state).
    // Returns 0 if no tracked TB window is under the cursor.
    uint64_t GetHoveredTBWindowZ();
}
