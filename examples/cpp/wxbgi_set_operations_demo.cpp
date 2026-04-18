/**
 * @file wxbgi_set_operations_demo.cpp
 * @brief Demonstrates set operations on 3D solids using wxBGI.
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
#include <cstdlib>
#include <cstring>

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

static void SetupCamerasAndOverlays(int panelW, int panelH);
static void BuildDdsScene(std::array<int, kGradSteps>& gradColors, bool testMode);


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


enum { ID_TIMER = wxID_HIGHEST + 1 };


class DemoFrame : public wxFrame
{
public:
    DemoFrame(bool testMode, int panelW, int panelH);

private:
    CameraPanel* m_panel  = nullptr;
    wxTimer*     m_timer    = nullptr;
    CameraPanel* m_cam3d    = nullptr;

    float m_az3d   = 45.f,  m_el3d   = 50.f;
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


class DemoApp : public wxApp
{
public:
    bool OnInit() override;
    int  OnRun()  override;
private:
    bool m_testMode = false;
};

wxIMPLEMENT_APP(DemoApp);



DemoFrame::DemoFrame(bool testMode, int panelW, int panelH)
    : wxFrame(nullptr, wxID_ANY,
              "wxbgi Set-Operations Demo \xe2\x80\x94 One Camera",
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
    SetStatusText("Arrows=orbit cam3d "
                  " |  1-3=cam3d shading  |  Click=select  Del=delete");

    // Single camera panel
    m_cam3d   = new CameraPanel(this, "cam3d",    m_shading3d);

    for (auto* p : {m_cam3d})
        p->SetMinSize(wxSize(panelW, panelH));

    auto* grid = new wxGridSizer(1, 1, 2, 2);
    grid->Add(m_cam3d,   1, wxEXPAND);
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

void DemoFrame::RegisterHuds()
{
    m_cam3d->SetHud([this]() {
        char line1[96];
        char line2[96];
        const int sel = wxbgi_selection_count();
        std::snprintf(line1, sizeof(line1),
            "cam3d  set operations demo  [%s]",
            m_shading3d == WXBGI_SOLID_WIREFRAME ? "wire" :
            (m_shading3d == WXBGI_SOLID_FLAT ? "flat" : "smooth"));
        std::snprintf(line2, sizeof(line2),
            "az=%.0f  el=%.0f  |  selected=%d  |  Arrows orbit, 1/2/3 shading",
            m_az3d, m_el3d, sel);
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(15);
        outtextxy(6, 6, line1);
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
        m_cam3d->MarkDirty();
    }
    prevDel = curDel;

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


    // cam3d shading 1/2/3 — edge-detect
    static bool p1=false,p2=false,p3=false,p4=false,p5=false,p6=false;
    bool c1=wxbgi_is_key_down(49)!=0, c2=wxbgi_is_key_down(50)!=0, c3=wxbgi_is_key_down(51)!=0;
    if (c1&&!p1) { m_shading3d  = WXBGI_SOLID_SMOOTH;    m_cam3d ->SetShadingMode(m_shading3d);  }
    if (c2&&!p2) { m_shading3d  = WXBGI_SOLID_FLAT;      m_cam3d ->SetShadingMode(m_shading3d);  }
    if (c3&&!p3) { m_shading3d  = WXBGI_SOLID_WIREFRAME; m_cam3d ->SetShadingMode(m_shading3d);  }
    p1=c1; p2=c2; p3=c3; 

    // Selection-count change → all scene_3d panels need a refresh
    const int sel = wxbgi_selection_count();
    if (sel != m_prevSel)
    {
        m_prevSel = sel;
        m_cam3d->MarkDirty();
    }

    // Flash animation (~20 fps), only when objects are selected
    if (sel > 0)
    {
        const double now = wxbgi_get_time_seconds();
        if (now - m_lastFlash >= 0.05)
        {
            m_lastFlash = now;
            m_cam3d->MarkDirty();
        }
    }

    // Mouse move → panels with cursor overlays need a refresh
    if (wxbgi_mouse_moved())
        m_cam3d->MarkDirty();
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
        std::printf("wxbgi_set_operations_demo_cpp: test init OK\n");
        return 0;
    }
    return wxApp::OnRun();
}

// ---------------------------------------------------------------------------
// Camera and overlay setup— must be called AFTER wxbgi_wx_init_for_canvas
// ---------------------------------------------------------------------------

static void SetupCamerasAndOverlays(int panelW, int panelH)
{
    // 3D perspective orbit (eye set by DemoFrame::UpdateOrbit3d).
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 55.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", 0, 0, panelW, panelH);

    // Overlays — grid and UCS axes on all cameras.
    wxbgi_overlay_grid_enable();
    wxbgi_overlay_grid_set_spacing(25.f);
    wxbgi_overlay_grid_set_extent(5);

    wxbgi_overlay_ucs_axes_enable();
    wxbgi_overlay_ucs_axes_set_length(120.f);

    // Selection cursor on the three interactive cameras.
    wxbgi_overlay_cursor_enable("cam3d");
    wxbgi_overlay_cursor_set_size("cam3d", 14);
    wxbgi_overlay_cursor_set_color("cam3d", 1);
}


static void BuildDdsScene(std::array<int, kGradSteps>& gradColors, bool testMode)
{
    wxbgi_dds_scene_create("scene_3d");
    wxbgi_dds_scene_set_active("scene_3d");

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

    // Set operations - Start

    const int unionColor = wxbgi_alloc_color(255, 255, 0);
    wxbgi_solid_set_face_color(unionColor);
    wxbgi_solid_set_edge_color(unionColor);
    wxbgi_solid_box(0.f, 70.f, 0.f, 20.f, 20.f, 20.f);
    const std::string boxA = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_solid_box(10.f, 70.f, 0.f, 20.f, 20.f, 20.f);
    const std::string boxB = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    /* move one of them through a retained transform node */
    const std::string movedB = wxbgi_dds_translate(boxB.c_str(), 5.f, 0.f, 0.f);
    /* create a retained union node */
    const char *ops[2] = { boxA.c_str(), movedB.c_str() };
    wxbgi_dds_set_visible(wxbgi_dds_union(2, ops), 1);

    const int isectColor = wxbgi_alloc_color(0, 255, 255);
    wxbgi_solid_set_face_color(isectColor);
    wxbgi_solid_set_edge_color(isectColor);
    wxbgi_solid_box(55.f, 70.f, 0.f, 24.f, 24.f, 24.f);
    const std::string isectA = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_solid_box(67.f, 70.f, 0.f, 24.f, 24.f, 24.f);
    const std::string isectB = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    const char *ops2[2] = { isectA.c_str(), isectB.c_str() };
    wxbgi_dds_set_visible(wxbgi_dds_intersection(2, ops2), 1);
    
    const int diffColor = wxbgi_alloc_color(255, 0, 0);
    wxbgi_solid_set_face_color(diffColor);
    wxbgi_solid_set_edge_color(diffColor);
    wxbgi_solid_cylinder(-55.f, 70.f, 0.f, 15.f, 30.f, cylSl, 1);
    const std::string cyl2 = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_solid_cylinder(-55.f, 70.f, 6.f, 10.f, 30.f, cylSl, 1);
    const std::string cyl3 = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    const char *ops3[2] = { cyl2.c_str(), cyl3.c_str() };
    wxbgi_dds_set_visible(wxbgi_dds_difference(2, ops3), 1);

    const int diffColor2 = wxbgi_alloc_color(170, 255, 120);
    wxbgi_solid_set_face_color(diffColor2);
    wxbgi_solid_set_edge_color(diffColor2);
    wxbgi_solid_sphere(38.f, 95.f, 0.f, 12.f, sphSl, sphSt);
    const std::string diffSphere = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_solid_torus(38.f, 95.f, 6.f, 9.f, 6.f, torTu, torSi);
    const std::string diffTorus = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    const char *opsDiff2[2] = { diffSphere.c_str(), diffTorus.c_str() };
    const std::string sphereTorusDiff = wxbgi_dds_difference(2, opsDiff2);
    wxbgi_dds_set_visible(sphereTorusDiff.c_str(), 1);
    wxbgi_dds_set_label(sphereTorusDiff.c_str(), "sphere-torus-diff");

    const int isectColor2 = wxbgi_alloc_color(128, 255, 255);
    wxbgi_solid_set_face_color(isectColor2);
    wxbgi_solid_set_edge_color(isectColor2);
    wxbgi_solid_cone(0.f, 95.f, 0.f, 10.f, 30.f, coneSl, 1);
    const std::string isectC = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_solid_sphere(0.f, 95.f, 0.f, 10.f, sphSl, sphSt);
    const std::string isectD = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    const char *ops4[2] = { isectC.c_str(), isectD.c_str() };
    const std::string coneSphereIsect = wxbgi_dds_intersection(2, ops4);
    wxbgi_dds_set_visible(coneSphereIsect.c_str(), 1);
    wxbgi_dds_set_label(coneSphereIsect.c_str(), "cone-sphere-isect");

    // Set operations - End

    // Assign cameras to scene_3d.
    wxbgi_cam_set_scene("cam3d", "scene_3d");

    wxbgi_dds_scene_set_active("scene_3d");
    // Solid constructors render immediately as they append to DDS. Clear that
    // construction-time preview so the first visible frame shows only the
    // retained DDS scene (including set-operation results) rendered in PreBlit().
    cleardevice();
}

