#pragma once

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameContainers/GamePos.h>

#include <Timer.h>
#include <Color.h>

#include <Widgets/Minimap/VBuffer.h>

class PingsLinesRenderer : public VBuffer {
    friend class Minimap;
    const float drawing_scale = 96.0f;
    const clock_t drawing_timeout = 5000;

    struct DrawingLine {
        DrawingLine() : start(TIMER_INIT()) {}
        clock_t start;
        float x1 = 0, y1 = 0;
        float x2 = 0, y2 = 0;
    };
    struct PlayerDrawing {
        PlayerDrawing() : player(0), session(0) {}
        DWORD player;
        DWORD session;
        std::deque<DrawingLine> lines;
    };
    struct Ping {
        Ping() : start(TIMER_INIT()) {}
        virtual ~Ping() = default;
        clock_t start;
        int duration = 3000;
        virtual float GetX() const = 0;
        virtual float GetY() const = 0;
        virtual float GetScale() const { return 1.0f; }
        virtual bool ShowInner() const { return true; }
        virtual DWORD GetAgentID() const { return 0; }
    };
    struct TerrainPing : Ping {
        TerrainPing(float _x, float _y) : Ping(), x(_x), y(_y) {}
        const float x, y;
        float GetX() const override { return x; }
        float GetY() const override { return y; }
        float GetScale() const override { return 2.0f; }
    };
    struct AgentPing : Ping {
        AgentPing(DWORD _id) : Ping(), id(_id) {}
        DWORD id;
        float GetX() const override;
        float GetY() const override;
        float GetScale() const override;
        DWORD GetAgentID() const override { return id; }
    };
    struct ClickPing : Ping {
        ClickPing(float _x, float _y) : Ping(), x(_x), y(_y) {
            start = TIMER_INIT() - 200;
            duration = 1000;
        }
        const float x, y;
        float GetX() const override { return x; }
        float GetY() const override { return y; }
        float GetScale() const override { return 0.08f; }
        bool ShowInner() const override { return false; }
    };
    class PingCircle : public VBuffer {
        void Initialize(IDirect3DDevice9* device) override;
    public:
        Color color = 0;
    };
    class Marker : public VBuffer {
        void Initialize(IDirect3DDevice9* device) override;
    public:
        Color color = 0;
    };

public:
    PingsLinesRenderer();

    void Render(IDirect3DDevice9* device) override;

    void Invalidate() override {
        VBuffer::Invalidate();
        ping_circle.Invalidate();
        for (Ping* p : pings) delete p;
        pings.clear();
    }

    bool OnMouseDown(float x, float y);
    bool OnMouseMove(float x, float y);
    bool OnMouseUp();
    void AddMouseClickPing(GW::Vec2f pos);

    void P046Callback(GW::Packet::StoC::AgentPinged *pak);
    void P138Callback(GW::Packet::StoC::CompassEvent *pak);
    void P153Callback(GW::Packet::StoC::GenericValueTarget *pak);
    void P221Callback(GW::Packet::StoC::SkillActivate *pak);

    void DrawSettings();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;

private:
    void Initialize(IDirect3DDevice9* device) override;

    void DrawPings(IDirect3DDevice9* device);
    void DrawShadowstepMarker(IDirect3DDevice9* device);
    void DrawShadowstepLine(IDirect3DDevice9* device);
    void DrawRecallLine(IDirect3DDevice9* device);
    void DrawDrawings(IDirect3DDevice9* device);
    void EnqueueVertex(float x, float y, Color color);

    short ToShortPos(float n) {
        return static_cast<short>(std::lroundf(n / drawing_scale));
    }

    void BumpSessionID() { if (--session_id < 0) session_id = 7; }
    void SendQueue();

    PingCircle ping_circle;
    std::deque<Ping*> pings;

    std::map<DWORD, PlayerDrawing> drawings;

    // for pings and drawings
    const long show_interval = 10;
    const long queue_interval = 25;
    const long send_interval = 250;
    bool mouse_down = false;
    bool mouse_moved = false; // true if moved since last pressed
    float mouse_x = 0.f;
    float mouse_y = 0.f;
    int session_id = 0;
    clock_t lastshown = 0;
    clock_t lastsent = 0;
    clock_t lastqueued = 0;
    std::vector<GW::UI::CompassPoint> queue;

    Color color_drawings = 0;
    Color color_shadowstep_line = Colors::ARGB(155, 128, 0, 128);
    Color color_shadowstep_line_maxrange = Colors::ARGB(255, 255, 0, 128);
    float maxrange_interp_begin = 0.85f;
    float maxrange_interp_end = 0.95f;
    bool reduce_ping_spam = false;

    // for markers
    Marker marker;
    DWORD recall_target = 0;

    // for the gpu
    D3DVertex *vertices = nullptr;  // vertices array
    unsigned int vertices_count = 0;// count of vertices
    unsigned int vertices_max = 0;  // max number of vertices to draw in one call
};
