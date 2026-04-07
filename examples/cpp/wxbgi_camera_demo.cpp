/**
 * wxbgi_camera_demo.cpp
 *
 * Four independent WxBgiCanvas panels — each one a real wx widget with its
 * own GL context — all rendering the same "scene_3d" DDS scene graph from a
 * different camera perspective.  There is NO shared canvas subdivided with
 * setviewport; every panel is a first-class wxGLCanvas child of the frame.
 *
 * Architecture
 * ─────────────
 *   CameraPanel : WxBgiCanvas
 *     Overrides PreBlit(w, h) — the virtual hook added to WxBgiCanvas —
 *     which fires inside Render() after the GL context is current and BGI
 *     is ready, but *before* the buffer blit.  PreBlit clears the shared
 *     BGI scratch buffer, renders the camera's scene into it, draws the HUD,
 *     then returns so the base-class blit copies it to THIS panel's GL surface.
 *     Because wx paints are sequential on the main thread the scratch buffer
 *     is never touched by two panels simultaneously.
 *
 * Layout (1440 × 960 window, 720 × 480 per panel):
 *
 *   ┌──────────────────┬──────────────────┐
 *   │  cam_left        │  cam2d           │
 *   │  top-down ortho  │  2D pan/zoom/rot │
 *   │  (read-only)     │  WASD / +- / QE  │
 *   ├──────────────────┼──────────────────┤
 *   │  cam3d           │  cam_iso         │
 *   │  3D perspective  │  3D perspective  │
 *   │  Arrow keys      │  I/K/J/L         │
 *   └──────────────────┴──────────────────┘
 *
 * All 4 cameras share "scene_3d"  (1 DDS graph → 4 cameras / viewports).
 *
 * Controls
 * ─────────
 *   Arrow keys        Orbit cam3d
 *   I / K             Elevation cam_iso  (up / down)
 *   J / L             Azimuth   cam_iso  (left / right)
 *   W / S / A / D     Pan cam2d
 *   + / -             Zoom cam2d
 *   Q / E             Rotate cam2d
 *   1 / 2 / 3         cam3d  shading: Smooth / Flat / Wireframe
 *   4 / 5 / 6         cam_iso shading: Smooth / Flat / Wireframe
 *   Click             Select DDS object (flashes orange)
 *   Ctrl+click        Multi-select
 *   Delete            Remove selected objects
 *   Escape            Quit
 */

#include <wx/wx.h>
#include <wx/timer.h>
#include "wx_bgi_wx.h"        // WxBgiCanvas
#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_overlay.h"
#include "wx_bgi_solid.h"
#include "wx_bgi_dds.h"
#include "bgi_types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Scene geometry constants
// ---------------------------------------------------------------------------
static constexpr float kRx1 = -80.f, kRy1 = -60.f;
static constexpr float kRx2 =  80.f, kRy2 =  60.f;

static constexpr float kCylX =  90.f, kCylY =   0.f, kCylZ = 0.f;
static constexpr float kCylR =  22.f, kCylH =  80.f;

static constexpr float kSphX = -90.f, kSphY =   0.f, kSphZ = 30.f;
static constexpr float kSphR =  28.f;

static constexpr float kBoxX =   0.f, kBoxY = -100.f, kBoxZ =  0.f;
static constexpr float kBoxW =  50.f, kBoxD =   50.f, kBoxH = 60.f;

static constexpr float kConeX =  80.f, kConeY = -100.f, kConeZ = 0.f;
static constexpr float kConeR =  28.f, kConeH =   75.f;

static constexpr float kTorX  = -80.f, kTorY  = -100.f, kTorZ  = 18.f;
static constexpr float kTorMaj =  28.f, kTorMin =   9.f;

static constexpr int   kGradSteps = 14;
static constexpr float kOrbitR    = 350.f;

static const char* shadingName(int mode)
{
    switch (mode) {
    case WXBGI_SOLID_SMOOTH:    return "Smooth";
    case WXBGI_SOLID_FLAT:      return "Flat";
    case WXBGI_SOLID_WIREFRAME: return "Wire";
    default:                    return "?";
    }
}

// ---------------------------------------------------------------------------
// CameraPanel — WxBgiCanvas subclass that renders a single named camera
// ---------------------------------------------------------------------------
class CameraPanel : public wxbgi::WxBgiCanvas
{
public:
    CameraPanel(wxWindow* parent, const char* camName, int shadingMode)
        : wxbgi::WxBgiCanvas(parent, wxID_ANY)
        , m_camName(camName)
        , m_shadingMode(shadingMode)
    {}

    void SetShadingMode(int mode) { m_shadingMode = mode; Refresh(false); }
    void SetHud(std::function<void()> fn) { m_hud = std::move(fn); }
    void MarkDirty() { Refresh(false); }

protected:
    // PreBlit is called by WxBgiCanvas::Render() after the GL context is
    // current and BGI is initialised, but before the buffer blit.
    void PreBlit(int w, int h) override
    {
        // Keep camera viewport in sync with this panel's current pixel size
        // (handles window resize automatically).
        wxbgi_cam_set_screen_viewport(m_camName.c_str(), 0, 0, w, h);

        // Sync active DDS scene to this camera's assigned scene and apply
        // the camera's shading mode.
        const char* scene = wxbgi_cam_get_scene(m_camName.c_str());
        if (scene && *scene)
        {
            wxbgi_dds_scene_set_active(scene);
            wxbgi_dds_set_solid_draw_mode(m_shadingMode);
        }

        // Clear the shared BGI scratch buffer and render this camera's view.
        cleardevice();
        setviewport(0, 0, w - 1, h - 1, 1);
        wxbgi_render_dds(m_camName.c_str());
        setviewport(0, 0, w - 1, h - 1, 0);   // restore without clip

        // Optional HUD overlay (labels, angle readouts, etc.).
        if (m_hud)
            m_hud();
    }

    // PostBlit is called after the 3-D solid GL pass.  Draw visual-aids overlays
    // (concentric circles, grid, UCS axes, selection cursor) on top of all 3-D
    // primitives by alpha-compositing a freshly rendered overlay layer.
    void PostBlit(int pageW, int pageH, int vpW, int vpH) override
    {
        wxbgi_wx_render_overlays_for_camera(m_camName.c_str(), pageW, pageH, vpW, vpH);
    }

private:
    std::string           m_camName;
    int                   m_shadingMode;
    std::function<void()> m_hud;
};

// ---------------------------------------------------------------------------
// Forward declarations for scene/camera setup (defined after DemoFrame)
// ---------------------------------------------------------------------------
static void SetupCamerasAndOverlays(int panelW, int panelH);
static void BuildDdsScene(std::array<int, kGradSteps>& gradColors, bool testMode);

// ---------------------------------------------------------------------------
// DemoFrame — 2×2 grid of four CameraPanel instances
// ---------------------------------------------------------------------------
enum { ID_TIMER = wxID_HIGHEST + 1 };

class DemoFrame : public wxFrame
{
public:
    DemoFrame(bool testMode, int panelW, int panelH);

private:
    CameraPanel* m_camLeft  = nullptr;
    CameraPanel* m_cam2d    = nullptr;
    CameraPanel* m_cam3d    = nullptr;
    CameraPanel* m_camIso   = nullptr;
    wxTimer*     m_timer    = nullptr;

    float m_az3d   = 45.f,  m_el3d   = 30.f;
    float m_azIso  = 225.f, m_elIso  = 22.f;
    int   m_shading3d  = WXBGI_SOLID_SMOOTH;
    int   m_shadingIso = WXBGI_SOLID_FLAT;
    int   m_prevSel    = 0;
    double m_lastFlash = 0.0;

    void UpdateOrbit3d();
    void UpdateOrbitIso();
    void RegisterHuds();
    void OnTimer(wxTimerEvent&);
    void OnClose(wxCloseEvent& evt) { if (m_timer) m_timer->Stop(); evt.Skip(); }
};

// ---------------------------------------------------------------------------
// DemoApp
// ---------------------------------------------------------------------------
class DemoApp : public wxApp
{
public:
    bool OnInit() override;
    int  OnRun()  override;
private:
    bool m_testMode = false;
};

wxIMPLEMENT_APP(DemoApp);

// ===========================================================================
// Implementation
// ===========================================================================

// ---- DemoFrame constructor -------------------------------------------------

DemoFrame::DemoFrame(bool testMode, int panelW, int panelH)
    : wxFrame(nullptr, wxID_ANY,
              "wx_bgi 4-Panel Demo \xe2\x80\x94 One Scene, Four Cameras",
              wxDefaultPosition, wxDefaultSize)
{
    // Menu bar
    auto* fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT, "&Quit\tEsc");
    auto* mb = new wxMenuBar;
    mb->Append(fileMenu, "&File");
    SetMenuBar(mb);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

    // Status bar (single field — key legend)
    CreateStatusBar(1);
    SetStatusText("Arrows=orbit cam3d  IJKL=orbit cam_iso  WASD/+-/QE=pan/zoom/rot cam2d"
                  "  |  1-3=cam3d shading  4-6=cam_iso shading  |  Click=select  Del=delete");

    // Four camera panels
    m_camLeft = new CameraPanel(this, "cam_left", WXBGI_SOLID_FLAT);
    m_cam2d   = new CameraPanel(this, "cam2d",    WXBGI_SOLID_WIREFRAME);
    m_cam3d   = new CameraPanel(this, "cam3d",    m_shading3d);
    m_camIso  = new CameraPanel(this, "cam_iso",  m_shadingIso);

    for (auto* p : {m_camLeft, m_cam2d, m_cam3d, m_camIso})
        p->SetMinSize(wxSize(panelW, panelH));

    // 2×2 grid with 2 px gaps; frame auto-sizes to fit the 4 panels.
    auto* grid = new wxGridSizer(2, 2, 2, 2);
    grid->Add(m_camLeft, 1, wxEXPAND);
    grid->Add(m_cam2d,   1, wxEXPAND);
    grid->Add(m_cam3d,   1, wxEXPAND);
    grid->Add(m_camIso,  1, wxEXPAND);
    SetSizerAndFit(grid);
    SetBackgroundColour(wxColour(24, 24, 24));   // dark gap colour

    // BGI is already initialised in DemoApp::OnInit() before this constructor
    // runs, so cameras and the DDS scene can be set up right here.
    SetupCamerasAndOverlays(panelW, panelH);
    UpdateOrbit3d();
    UpdateOrbitIso();

    std::array<int, kGradSteps> gradColors{};
    BuildDdsScene(gradColors, testMode);

    RegisterHuds();

    Bind(wxEVT_CLOSE_WINDOW, &DemoFrame::OnClose, this);

    // In test mode the timer must NOT run: wxYieldIfNeeded() already triggers
    // the first paint, and the timer firing during wxWidgets cleanup (after
    // OnRun returns 0) would dereference panel pointers that are being
    // destroyed — causing a segfault on Linux.
    if (!testMode)
    {
        m_timer = new wxTimer(this, ID_TIMER);
        Bind(wxEVT_TIMER, &DemoFrame::OnTimer, this, ID_TIMER);
        m_timer->Start(16);
    }
}

// ---- Orbit helpers ---------------------------------------------------------

void DemoFrame::UpdateOrbit3d()
{
    const float az = m_az3d * (float)M_PI / 180.f;
    const float el = m_el3d * (float)M_PI / 180.f;
    wxbgi_cam_set_eye   ("cam3d", kOrbitR*cosf(el)*cosf(az),
                                  kOrbitR*cosf(el)*sinf(az),
                                  kOrbitR*sinf(el));
    wxbgi_cam_set_target("cam3d", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up    ("cam3d", 0.f, 0.f, 1.f);
}

void DemoFrame::UpdateOrbitIso()
{
    const float az = m_azIso * (float)M_PI / 180.f;
    const float el = m_elIso * (float)M_PI / 180.f;
    wxbgi_cam_set_eye   ("cam_iso", kOrbitR*cosf(el)*cosf(az),
                                    kOrbitR*cosf(el)*sinf(az),
                                    kOrbitR*sinf(el));
    wxbgi_cam_set_target("cam_iso", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up    ("cam_iso", 0.f, 0.f, 1.f);
}

// ---- HUD lambdas -----------------------------------------------------------

void DemoFrame::RegisterHuds()
{
    // cam_left: fixed title — scene never changes so rarely re-renders.
    m_camLeft->SetHud([]() {
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(15);
        outtextxy(6, 6,  const_cast<char*>("cam_left  [scene_3d]  top-down 2D ortho"));
        setcolor(8);
        outtextxy(6, 20, const_cast<char*>("(read-only — all scene changes visible here)"));
    });

    // cam2d: live pan / zoom / rotation readout.
    m_cam2d->SetHud([]() {
        float px = 0.f, py = 0.f, zoom = 1.f, rot = 0.f;
        wxbgi_cam2d_get_pan     ("cam2d", &px, &py);
        wxbgi_cam2d_get_zoom    ("cam2d", &zoom);
        wxbgi_cam2d_get_rotation("cam2d", &rot);
        char buf[96];
        std::snprintf(buf, sizeof(buf),
            "pan=(%.0f,%.0f)  zoom=%.2f  rot=%.0f\xc2\xb0  | WASD +- QE",
            px, py, zoom, rot);
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(11);
        outtextxy(6, 6,  const_cast<char*>("cam2d  [scene_3d]  interactive 2D pan / zoom / rotate"));
        setcolor(8);
        outtextxy(6, 20, buf);
    });

    // cam3d: orbit state + shading + selection count.
    m_cam3d->SetHud([this]() {
        char line1[96], line2[96], line3[96];
        std::snprintf(line1, sizeof(line1),
            "cam3d  [scene_3d]  perspective orbit  [%s]", shadingName(m_shading3d));
        std::snprintf(line2, sizeof(line2),
            "az=%.0f  el=%.0f  |  Arrow keys  |  1=Smooth  2=Flat  3=Wire",
            m_az3d, m_el3d);
        const int sel = wxbgi_selection_count();
        if (sel > 0)
            std::snprintf(line3, sizeof(line3),
                "Selected: %d  (Del=delete  Ctrl+click=multi-select)", sel);
        else
            std::snprintf(line3, sizeof(line3),
                "Click to select objects  |  Ctrl+click = multi-select  |  Del = delete");
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(15);
        outtextxy(6, 6,  line1);
        setcolor(8);
        outtextxy(6, 20, line2);
        setcolor(sel > 0 ? 14 : 8);
        outtextxy(6, 34, line3);
    });

    // cam_iso: orbit state + shading.
    m_camIso->SetHud([this]() {
        char line1[96], line2[96];
        std::snprintf(line1, sizeof(line1),
            "cam_iso  [scene_3d]  perspective orbit  [%s]", shadingName(m_shadingIso));
        std::snprintf(line2, sizeof(line2),
            "az=%.0f  el=%.0f  |  J/L=azimuth  I/K=elevation  |  4=Smooth  5=Flat  6=Wire",
            m_azIso, m_elIso);
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(15);
        outtextxy(6, 6,  line1);
        setcolor(8);
        outtextxy(6, 20, line2);
    });
}

// ---- Timer handler — input polling + dirty-flag dispatch -------------------

void DemoFrame::OnTimer(wxTimerEvent&)
{
    wxbgi_poll_events();

    if (wxbgi_is_key_down(WXBGI_KEY_ESCAPE)) { Close(); return; }

    // Delete selected (edge-detect so one press = one action)
    static bool prevDel = false;
    const bool  curDel  = wxbgi_is_key_down(WXBGI_KEY_DELETE) != 0;
    if (curDel && !prevDel)
    {
        wxbgi_selection_delete_selected();
        m_camLeft->MarkDirty(); m_cam2d->MarkDirty();
        m_cam3d->MarkDirty();   m_camIso->MarkDirty();
    }
    prevDel = curDel;

    // cam2d navigation (continuous while held)
    bool d2 = false;
    if (wxbgi_is_key_down(87))  { wxbgi_cam2d_pan_by("cam2d",  0.f,  5.f); d2=true; } // W
    if (wxbgi_is_key_down(83))  { wxbgi_cam2d_pan_by("cam2d",  0.f, -5.f); d2=true; } // S
    if (wxbgi_is_key_down(65))  { wxbgi_cam2d_pan_by("cam2d", -5.f,  0.f); d2=true; } // A
    if (wxbgi_is_key_down(68))  { wxbgi_cam2d_pan_by("cam2d",  5.f,  0.f); d2=true; } // D
    if (wxbgi_is_key_down(WXBGI_KEY_EQUAL)  || wxbgi_is_key_down(334))
        { wxbgi_cam2d_zoom_at("cam2d", 1.02f, 0.f, 0.f); d2=true; }
    if (wxbgi_is_key_down(WXBGI_KEY_MINUS) || wxbgi_is_key_down(335))
        { wxbgi_cam2d_zoom_at("cam2d", 0.98f, 0.f, 0.f); d2=true; }
    if (wxbgi_is_key_down(81)) // Q
    {
        float r=0.f; wxbgi_cam2d_get_rotation("cam2d",&r);
        wxbgi_cam2d_set_rotation("cam2d", r+1.f); d2=true;
    }
    if (wxbgi_is_key_down(69)) // E
    {
        float r=0.f; wxbgi_cam2d_get_rotation("cam2d",&r);
        wxbgi_cam2d_set_rotation("cam2d", r-1.f); d2=true;
    }
    if (d2) m_cam2d->MarkDirty();

    // cam3d orbit — arrow keys
    bool d3 = false;
    if (wxbgi_is_key_down(WXBGI_KEY_LEFT))
        { m_az3d -= 1.f; d3=true; }
    if (wxbgi_is_key_down(WXBGI_KEY_RIGHT))
        { m_az3d += 1.f; d3=true; }
    if (wxbgi_is_key_down(WXBGI_KEY_UP))
        { m_el3d = std::min(89.f, m_el3d+1.f); d3=true; }
    if (wxbgi_is_key_down(WXBGI_KEY_DOWN))
        { m_el3d = std::max(-89.f, m_el3d-1.f); d3=true; }
    if (d3) { UpdateOrbit3d(); m_cam3d->MarkDirty(); }

    // cam_iso orbit — J/L/I/K
    bool dIso = false;
    if (wxbgi_is_key_down(74)) { m_azIso -= 1.f; dIso=true; }               // J
    if (wxbgi_is_key_down(76)) { m_azIso += 1.f; dIso=true; }               // L
    if (wxbgi_is_key_down(73)) { m_elIso=std::min(89.f,  m_elIso+1.f); dIso=true; } // I
    if (wxbgi_is_key_down(75)) { m_elIso=std::max(-89.f, m_elIso-1.f); dIso=true; } // K
    if (dIso) { UpdateOrbitIso(); m_camIso->MarkDirty(); }

    // cam3d shading 1/2/3 — edge-detect
    static bool p1=false,p2=false,p3=false,p4=false,p5=false,p6=false;
    bool c1=wxbgi_is_key_down(49)!=0, c2=wxbgi_is_key_down(50)!=0, c3=wxbgi_is_key_down(51)!=0;
    bool c4=wxbgi_is_key_down(52)!=0, c5=wxbgi_is_key_down(53)!=0, c6=wxbgi_is_key_down(54)!=0;
    if (c1&&!p1) { m_shading3d  = WXBGI_SOLID_SMOOTH;    m_cam3d ->SetShadingMode(m_shading3d);  }
    if (c2&&!p2) { m_shading3d  = WXBGI_SOLID_FLAT;      m_cam3d ->SetShadingMode(m_shading3d);  }
    if (c3&&!p3) { m_shading3d  = WXBGI_SOLID_WIREFRAME; m_cam3d ->SetShadingMode(m_shading3d);  }
    if (c4&&!p4) { m_shadingIso = WXBGI_SOLID_SMOOTH;    m_camIso->SetShadingMode(m_shadingIso); }
    if (c5&&!p5) { m_shadingIso = WXBGI_SOLID_FLAT;      m_camIso->SetShadingMode(m_shadingIso); }
    if (c6&&!p6) { m_shadingIso = WXBGI_SOLID_WIREFRAME; m_camIso->SetShadingMode(m_shadingIso); }
    p1=c1; p2=c2; p3=c3; p4=c4; p5=c5; p6=c6;

    // Selection-count change → all scene_3d panels need a refresh
    const int sel = wxbgi_selection_count();
    if (sel != m_prevSel)
    {
        m_prevSel = sel;
        m_camLeft->MarkDirty(); m_cam3d->MarkDirty(); m_camIso->MarkDirty();
    }

    // Flash animation (~20 fps), only when objects are selected
    if (sel > 0)
    {
        const double now = wxbgi_get_time_seconds();
        if (now - m_lastFlash >= 0.05)
        {
            m_lastFlash = now;
            m_camLeft->MarkDirty(); m_cam3d->MarkDirty(); m_camIso->MarkDirty();
        }
    }

    // Mouse move → panels with cursor overlays need a refresh
    if (wxbgi_mouse_moved())
        m_cam2d->MarkDirty(), m_cam3d->MarkDirty(), m_camIso->MarkDirty();
}

// ---------------------------------------------------------------------------
// DemoApp::OnInit
// ---------------------------------------------------------------------------

bool DemoApp::OnInit()
{
    bool testMode = false;
    for (int i = 1; i < argc; ++i)
        if (wxString(argv[i]) == wxT("--test"))
            testMode = true;

    const int panelW = testMode ? 240 : 720;
    const int panelH = testMode ? 160 : 480;

    // Initialise BGI in wx-embedded mode.  Must happen before camera creation
    // because wxbgi_cam_create checks gState.wxEmbedded.
    wxbgi_wx_init_for_canvas(panelW, panelH);

    DemoFrame* frame = new DemoFrame(testMode, panelW, panelH);
    SetTopWindow(frame);
    frame->Show();
    m_testMode = testMode;
    return true;
}

int DemoApp::OnRun()
{
    if (m_testMode)
    {
        // Yield once so the first OnPaint/Render fires, then exit cleanly.
        wxYieldIfNeeded();
        std::printf("wxbgi_camera_demo_cpp: test init OK\n");
        return 0;
    }
    return wxApp::OnRun();
}

// ---------------------------------------------------------------------------
// Camera and overlay setup— must be called AFTER wxbgi_wx_init_for_canvas
// ---------------------------------------------------------------------------

static void SetupCamerasAndOverlays(int panelW, int panelH)
{
    // Every camera gets viewport (0, 0, panelW, panelH) because each camera
    // has its own full-size CameraPanel.  PreBlit() keeps this in sync on resize.

    // Top-left: fixed top-down 2D ortho (read-only).
    wxbgi_cam2d_create("cam_left");
    wxbgi_cam2d_set_world_height("cam_left", 300.f);
    wxbgi_cam2d_set_pan("cam_left", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam_left", 0, 0, panelW, panelH);

    // Top-right: interactive 2D pan / zoom / rotate.
    wxbgi_cam2d_create("cam2d");
    wxbgi_cam2d_set_world_height("cam2d", 320.f);
    wxbgi_cam2d_set_pan("cam2d", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam2d", 0, 0, panelW, panelH);

    // Bottom-left: 3D perspective orbit (eye set by DemoFrame::UpdateOrbit3d).
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 55.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", 0, 0, panelW, panelH);

    // Bottom-right: second 3D perspective (eye set by DemoFrame::UpdateOrbitIso).
    wxbgi_cam_create("cam_iso", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam_iso", 45.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam_iso", 0, 0, panelW, panelH);

    // Overlays — grid and UCS axes on all cameras.
    wxbgi_overlay_grid_enable();
    wxbgi_overlay_grid_set_spacing(25.f);
    wxbgi_overlay_grid_set_extent(5);

    wxbgi_overlay_ucs_axes_enable();
    wxbgi_overlay_ucs_axes_set_length(120.f);

    // Concentric circles on every camera.
    const char* allCams[] = {"cam_left", "cam2d", "cam3d", "cam_iso"};
    for (const char* cam : allCams)
    {
        wxbgi_overlay_concentric_enable(cam);
        wxbgi_overlay_concentric_set_count(cam, 3);
    }
    wxbgi_overlay_concentric_set_radii("cam_left", 40.f, 120.f);
    wxbgi_overlay_concentric_set_radii("cam2d",    40.f, 120.f);
    wxbgi_overlay_concentric_set_radii("cam3d",    60.f, 180.f);
    wxbgi_overlay_concentric_set_radii("cam_iso",  60.f, 180.f);

    // Selection cursor on the three interactive cameras.
    wxbgi_overlay_cursor_enable("cam2d");
    wxbgi_overlay_cursor_set_size("cam2d", 14);
    wxbgi_overlay_cursor_set_color("cam2d", 0);

    wxbgi_overlay_cursor_enable("cam3d");
    wxbgi_overlay_cursor_set_size("cam3d", 14);
    wxbgi_overlay_cursor_set_color("cam3d", 1);

    wxbgi_overlay_cursor_enable("cam_iso");
    wxbgi_overlay_cursor_set_size("cam_iso", 14);
    wxbgi_overlay_cursor_set_color("cam_iso", 2);
}

// ---------------------------------------------------------------------------
// DDS scene construction — one scene ("scene_3d"), all 4 cameras assigned
// ---------------------------------------------------------------------------

static void BuildDdsScene(std::array<int, kGradSteps>& gradColors, bool testMode)
{
    wxbgi_dds_scene_create("scene_3d");
    wxbgi_dds_scene_set_active("scene_3d");

    // Gradient-filled quad strips (orange → yellow).
    for (int i = 0; i < kGradSteps; ++i)
    {
        const float t = (float)i / (float)(kGradSteps - 1);
        gradColors[i] = wxbgi_alloc_color(255, (int)(255.f * t), 0);
    }
    for (int i = 0; i < kGradSteps; ++i)
    {
        const float y0  = kRy1 + (kRy2-kRy1)*(float) i      / (float)kGradSteps;
        const float y1  = kRy1 + (kRy2-kRy1)*(float)(i+1)   / (float)kGradSteps;
        const float xd0 = kRx1 + (kRx2-kRx1)*(y0-kRy1) / (kRy2-kRy1);
        const float xd1 = kRx1 + (kRx2-kRx1)*(y1-kRy1) / (kRy2-kRy1);
        const int   c   = gradColors[i];
        setcolor(c); setfillstyle(bgi::SOLID_FILL, c);
        float pts[12] = { kRx1,y0,0.f, xd0,y0,0.f, xd1,y1,0.f, kRx1,y1,0.f };
        wxbgi_world_fillpoly(pts, 4);
    }

    // Rectangle outline + diagonal guide.
    setcolor(15); wxbgi_world_rectangle(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);
    setcolor(8);  wxbgi_world_line(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);

    // World circle and text label at origin.
    setcolor(3);  wxbgi_world_circle(0.f, 0.f, 0.f, 55.f);
    setcolor(15); wxbgi_world_outtextxy(-10.f, -15.f, 0.f, "origin");

    // 3D solids (reduced tessellation in test mode for speed).
    const int cylSl  = testMode ?  6 : 12;
    const int sphSl  = testMode ?  6 : 12, sphSt = testMode ? 4 : 8;
    const int coneSl = testMode ?  6 : 12;
    const int torTu  = testMode ?  8 : 16, torSi = testMode ? 4 : 8;

    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(15);

    wxbgi_solid_set_face_color(wxbgi_alloc_color(255, 100,  20));  // orange-red
    wxbgi_solid_cylinder(kCylX, kCylY, kCylZ, kCylR, kCylH, cylSl, 1);

    wxbgi_solid_set_face_color(wxbgi_alloc_color(200,  40, 200));  // magenta
    wxbgi_solid_sphere(kSphX, kSphY, kSphZ, kSphR, sphSl, sphSt);

    wxbgi_solid_set_face_color(wxbgi_alloc_color( 40,  80, 220));  // cobalt blue
    wxbgi_solid_box(kBoxX, kBoxY, kBoxZ, kBoxW, kBoxD, kBoxH);

    wxbgi_solid_set_face_color(wxbgi_alloc_color( 20, 180, 180));  // teal
    wxbgi_solid_cone(kConeX, kConeY, kConeZ, kConeR, kConeH, coneSl, 1);

    wxbgi_solid_set_face_color(wxbgi_alloc_color( 60, 200,  60));  // lime green
    wxbgi_solid_torus(kTorX, kTorY, kTorZ, kTorMaj, kTorMin, torTu, torSi);

    // Assign all 4 cameras to scene_3d  (1 DDS graph → 4 cameras).
    wxbgi_cam_set_scene("cam_left", "scene_3d");
    wxbgi_cam_set_scene("cam2d",    "scene_3d");
    wxbgi_cam_set_scene("cam3d",    "scene_3d");
    wxbgi_cam_set_scene("cam_iso",  "scene_3d");

    wxbgi_dds_scene_set_active("scene_3d");
}

