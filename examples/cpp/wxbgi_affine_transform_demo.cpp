/**
 * @file wxbgi_affine_transform_demo.cpp
 * @brief Animated retained affine transform demo for 3D solids.
 */

#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/stopwatch.h>

#include "wx_bgi_wx.h"
#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_overlay.h"
#include "wx_bgi_solid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

constexpr int   kTimerMs = 33;
constexpr int   kAnimMs = 30000;
constexpr int   kTestExtraMs = 10000;
constexpr float kAzStartDeg = 36.f;
constexpr float kElStartDeg = 24.f;
constexpr float kRadiusStart = 340.f;

double wave(double t, double hz)
{
    return std::sin(t * hz * 2.0 * 3.14159265358979323846);
}

const char *shadingName(int mode)
{
    switch (mode)
    {
    case WXBGI_SOLID_WIREFRAME: return "Wire";
    case WXBGI_SOLID_FLAT:      return "Flat";
    case WXBGI_SOLID_SMOOTH:    return "Smooth";
    default:                    return "?";
    }
}

class CameraPanel : public wxbgi::WxBgiCanvas
{
public:
    explicit CameraPanel(wxWindow *parent)
        : wxbgi::WxBgiCanvas(parent, wxID_ANY)
    {}

    void SetHudLine1(std::string text) { m_line1 = std::move(text); }
    void SetHudLine2(std::string text) { m_line2 = std::move(text); }
    void SetShading(int mode) { m_shading = mode; Refresh(false); }
    void MarkDirty() { Refresh(false); }

protected:
    void PreBlit(int w, int h) override
    {
        wxbgi_cam_set_screen_viewport("affine_cam", 0, 0, w, h);
        wxbgi_dds_scene_set_active("affine_demo");
        wxbgi_dds_set_solid_draw_mode(m_shading);

        cleardevice();
        setviewport(0, 0, w - 1, h - 1, 1);
        wxbgi_render_dds("affine_cam");
        setviewport(0, 0, w - 1, h - 1, 0);

        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
        setcolor(bgi::WHITE);
        outtextxy(8, 8, const_cast<char *>(m_line1.c_str()));
        setcolor(bgi::LIGHTGRAY);
        outtextxy(8, 22, const_cast<char *>(m_line2.c_str()));
    }

    void PostBlit(int pageW, int pageH, int vpW, int vpH) override
    {
        wxbgi_wx_render_overlays_for_camera("affine_cam", pageW, pageH, vpW, vpH);
    }

private:
    int         m_shading{WXBGI_SOLID_SMOOTH};
    std::string m_line1;
    std::string m_line2;
};

enum { ID_TIMER = wxID_HIGHEST + 101 };

class DemoFrame : public wxFrame
{
public:
    explicit DemoFrame(bool testMode);

private:
    void UpdateCamera();
    void RebuildScene(double seconds);
    void OnTimer(wxTimerEvent &);
    void OnClose(wxCloseEvent &evt) { if (m_timer) m_timer->Stop(); evt.Skip(); }

    CameraPanel *m_panel{nullptr};
    wxTimer     *m_timer{nullptr};
    wxStopWatch  m_clock;
    bool         m_testMode{false};
    bool         m_manual{false};
    float        m_azDeg{kAzStartDeg};
    float        m_elDeg{kElStartDeg};
    float        m_radius{kRadiusStart};
    int          m_shading{WXBGI_SOLID_SMOOTH};
};

class DemoApp : public wxApp
{
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(DemoApp);

DemoFrame::DemoFrame(bool testMode)
    : wxFrame(nullptr, wxID_ANY,
              "wxbgi Affine Transform Demo",
              wxDefaultPosition,
              wxDefaultSize),
      m_testMode(testMode)
{
    auto *fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT, "&Quit\tEsc");
    auto *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { Close(); }, wxID_EXIT);

    CreateStatusBar(1);
    SetStatusText("Arrows orbit | +/- zoom | 1/2/3 shading | Esc quit");

    const int panelW = testMode ? 480 : 960;
    const int panelH = testMode ? 320 : 720;

    m_panel = new CameraPanel(this);
    m_panel->SetMinSize(wxSize(panelW, panelH));

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_panel, 1, wxEXPAND);
    SetSizerAndFit(sizer);
    SetBackgroundColour(wxColour(24, 24, 24));

    wxbgi_dds_scene_create("affine_demo");
    wxbgi_dds_scene_set_active("affine_demo");

    wxbgi_cam_create("affine_cam", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("affine_cam", "affine_demo");
    wxbgi_cam_set_perspective("affine_cam", 52.f, 0.5f, 5000.f);
    wxbgi_cam_set_target("affine_cam", 0.f, 0.f, 20.f);
    wxbgi_cam_set_up("affine_cam", 0.f, 0.f, 1.f);
    wxbgi_cam_set_screen_viewport("affine_cam", 0, 0, panelW, panelH);

    wxbgi_overlay_grid_enable();
    wxbgi_overlay_grid_set_spacing(25.f);
    wxbgi_overlay_grid_set_extent(7);
    wxbgi_overlay_ucs_axes_enable();
    wxbgi_overlay_ucs_axes_set_length(80.f);
    wxbgi_overlay_cursor_enable("affine_cam");
    wxbgi_overlay_cursor_set_size("affine_cam", 14);
    wxbgi_overlay_cursor_set_color("affine_cam", bgi::YELLOW);

    UpdateCamera();
    RebuildScene(0.0);
    m_panel->MarkDirty();

    Bind(wxEVT_CLOSE_WINDOW, &DemoFrame::OnClose, this);
    m_timer = new wxTimer(this, ID_TIMER);
    Bind(wxEVT_TIMER, &DemoFrame::OnTimer, this, ID_TIMER);
    m_timer->Start(kTimerMs);
}

void DemoFrame::UpdateCamera()
{
    const float az = m_azDeg * 3.14159265358979323846f / 180.f;
    const float el = m_elDeg * 3.14159265358979323846f / 180.f;
    const float x = m_radius * std::cos(el) * std::cos(az);
    const float y = m_radius * std::cos(el) * std::sin(az);
    const float z = m_radius * std::sin(el);
    wxbgi_cam_set_eye("affine_cam", x, y, z);
}

void DemoFrame::RebuildScene(double seconds)
{
    wxbgi_dds_scene_set_active("affine_demo");
    wxbgi_dds_clear();

    const int boxSl = m_testMode ? 10 : 16;
    (void)boxSl;
    const int sphereSlices = m_testMode ? 10 : 20;
    const int sphereStacks = m_testMode ? 8 : 16;
    const int cylSlices = m_testMode ? 12 : 24;
    const double translateOsc = 45.0 * wave(seconds, 0.18);
    const double translateLift = 10.0 * (wave(seconds, 0.32) + 1.0);
    const double rotateDeg = seconds * 90.0;
    const float scaleFactor = static_cast<float>(0.70 + 0.30 * (wave(seconds, 0.28) + 1.0));
    const float skewXY = static_cast<float>(0.65 * wave(seconds, 0.16));
    const float skewXZ = static_cast<float>(0.35 * wave(seconds, 0.11));
    const float skewYZ = static_cast<float>(0.25 * wave(seconds, 0.21));
    const float skewSpinDeg = static_cast<float>(18.0 * wave(seconds, 0.14));

    wxbgi_solid_set_draw_mode(m_shading);

    wxbgi_solid_set_face_color(bgi::LIGHTRED);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_box(0.f, 0.f, 0.f, 34.f, 34.f, 34.f);
    std::string translateId = wxbgi_dds_translate(
        std::string(wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1)).c_str(),
        static_cast<float>(-150.0 + translateOsc), -70.f, static_cast<float>(18.0 + translateLift));
    (void)translateId;

    wxbgi_solid_set_face_color(bgi::LIGHTGREEN);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_cylinder(0.f, 0.f, 0.f, 18.f, 72.f, cylSlices, 1);
    const std::string rotBase = std::string(wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1));
    const std::string rotA = wxbgi_dds_rotate_axis_deg(rotBase.c_str(), 1.f, 1.f, 0.4f, static_cast<float>(rotateDeg));
    const std::string rotB = wxbgi_dds_rotate_x_deg(rotA.c_str(), static_cast<float>(45.0 * wave(seconds, 0.19)));
    const std::string rotC = wxbgi_dds_translate(rotB.c_str(), 0.f, -65.f, 0.f);
    (void)rotC;

    wxbgi_solid_set_face_color(bgi::LIGHTCYAN);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_sphere(0.f, 0.f, 0.f, 24.f, sphereSlices, sphereStacks);
    const std::string scaleBase = std::string(wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1));
    const std::string scaleA = wxbgi_dds_scale_uniform(scaleBase.c_str(), scaleFactor);
    const std::string scaleB = wxbgi_dds_translate(scaleA.c_str(), 145.f, -70.f, 24.f);
    (void)scaleB;

    wxbgi_solid_set_face_color(bgi::LIGHTMAGENTA);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_box(0.f, 0.f, 0.f, 72.f, 28.f, 42.f);
    const std::string skewBase = std::string(wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1));
    const std::string skewA = wxbgi_dds_skew(skewBase.c_str(), skewXY, skewXZ, 0.f, skewYZ, 0.f, 0.f);
    const std::string skewB = wxbgi_dds_rotate_y_deg(skewA.c_str(), skewSpinDeg);
    const std::string skewC = wxbgi_dds_translate(skewB.c_str(), 0.f, 90.f, 28.f);
    (void)skewC;

    wxbgi_solid_set_face_color(bgi::DARKGRAY);
    wxbgi_solid_set_edge_color(bgi::LIGHTGRAY);
    wxbgi_solid_box(0.f, 0.f, -6.f, 420.f, 260.f, 4.f);
}

void DemoFrame::OnTimer(wxTimerEvent &)
{
    wxbgi_poll_events();

    if (wxbgi_is_key_down(WXBGI_KEY_ESCAPE))
    {
        Close();
        return;
    }

    const int elapsedMs = m_clock.Time();
    if (m_testMode && elapsedMs >= (kAnimMs + kTestExtraMs))
    {
        Close();
        return;
    }

    if (!m_manual && elapsedMs >= kAnimMs)
        m_manual = true;

    if (!m_manual)
    {
        const double seconds = elapsedMs / 1000.0;
        m_azDeg = static_cast<float>(kAzStartDeg + 28.0 * wave(seconds, 0.03));
        m_elDeg = static_cast<float>(kElStartDeg + 10.0 * wave(seconds, 0.05));
        m_radius = static_cast<float>(kRadiusStart - 40.0 * wave(seconds, 0.02));
    }
    else
    {
        if (wxbgi_is_key_down(WXBGI_KEY_LEFT))  m_azDeg -= 1.8f;
        if (wxbgi_is_key_down(WXBGI_KEY_RIGHT)) m_azDeg += 1.8f;
        if (wxbgi_is_key_down(WXBGI_KEY_UP))    m_elDeg = std::min(85.f, m_elDeg + 1.2f);
        if (wxbgi_is_key_down(WXBGI_KEY_DOWN))  m_elDeg = std::max(-10.f, m_elDeg - 1.2f);
        if (wxbgi_is_key_down('+') || wxbgi_is_key_down('='))
            m_radius = std::max(120.f, m_radius - 4.f);
        if (wxbgi_is_key_down('-') || wxbgi_is_key_down('_'))
            m_radius = std::min(650.f, m_radius + 4.f);
    }

    if (wxbgi_is_key_down('1')) m_shading = WXBGI_SOLID_WIREFRAME;
    if (wxbgi_is_key_down('2')) m_shading = WXBGI_SOLID_FLAT;
    if (wxbgi_is_key_down('3')) m_shading = WXBGI_SOLID_SMOOTH;

    UpdateCamera();
    RebuildScene(elapsedMs / 1000.0);

    char line1[160];
    char line2[192];
    std::snprintf(line1, sizeof(line1),
                  "Affine demo: translation, rotation, scale, skew  [%s]",
                  shadingName(m_shading));
    std::snprintf(line2, sizeof(line2),
                  "%s  t=%.1fs  az=%.0f  el=%.0f  zoom=%.0f  |  Arrows orbit  +/- zoom  1/2/3 shading",
                  m_manual ? "manual camera" : "scripted camera",
                  elapsedMs / 1000.0, m_azDeg, m_elDeg, m_radius);
    m_panel->SetHudLine1(line1);
    m_panel->SetHudLine2(line2);
    m_panel->SetShading(m_shading);
    m_panel->MarkDirty();
}

bool DemoApp::OnInit()
{
    bool testMode = false;
    for (int i = 1; i < argc; ++i)
        if (wxString(argv[i]) == wxT("--test"))
            testMode = true;

    const int panelW = testMode ? 480 : 960;
    const int panelH = testMode ? 320 : 720;
    wxbgi_wx_init_for_canvas(panelW, panelH);

    auto *frame = new DemoFrame(testMode);
    SetTopWindow(frame);
    frame->Show();
    return true;
}
