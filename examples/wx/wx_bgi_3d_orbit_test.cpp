// wx_bgi_3d_orbit_test.cpp
//
// Automated CTest: a 3-D perspective camera performs one complete 360-degree
// orbit over kDurationMs milliseconds (default 4500).
//
//   First half of orbit  : solids rendered WIREFRAME.
//   Second half          : solids rendered SMOOTH (GL Phong shading + depth buffer).
//
// Mode switch uses wxbgi_dds_set_solid_draw_mode() — fast in-place update,
// no JSON round-trip, no camera state wipe.  Scene is built once at startup.
//
// Frame advancement is TIME-BASED: m_frame tracks how far through the orbit
// we are based on wall-clock elapsed time, so the test always completes in
// approximately kDurationMs regardless of per-frame render cost.
//
// Run:
//   ctest --test-dir build -C Debug -R wx_bgi_3d_orbit_test --output-on-failure
// or:
//   .\build\Debug\wx_bgi_3d_orbit_test.exe

#include <wx/wx.h>
#include <wx/timer.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <string>

#include "wx_bgi_wx.h"
#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_solid.h"

using namespace bgi;

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Animation constants
// ---------------------------------------------------------------------------
static constexpr long  kDurationMs  = 4500;    // total orbit duration
static constexpr int   kTotalFrames = 90;       // logical frame count (display steps)
static constexpr int   kHalfFrames  = kTotalFrames / 2;
static constexpr float kElevDeg     = 28.0f;
static constexpr float kDist        = 14.0f;
static constexpr int   kTimerMs     = 50;       // timer poll interval (20 Hz cap)

// ---------------------------------------------------------------------------
// OrbitFrame
// ---------------------------------------------------------------------------
class OrbitFrame : public wxFrame
{
public:
    OrbitFrame()
        : wxFrame(nullptr, wxID_ANY, "wx_bgi 3-D Orbit Test",
                  wxDefaultPosition, wxSize(800, 640))
    {
        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                          wxDefaultPosition, wxSize(800, 600));
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND | wxALL, 4);
        SetSizer(sizer);
        m_canvas->Bind(wxEVT_PAINT, &OrbitFrame::OnFirstPaint, this);
        Bind(wxEVT_CLOSE_WINDOW, &OrbitFrame::OnClose, this);
    }

private:
    wxbgi::WxBgiCanvas *m_canvas  {nullptr};
    wxTimer             m_timer   {this, wxID_ANY};
    bool                m_ready   {false};
    bool                m_solid   {false};
    int                 m_canvasW {800}, m_canvasH {600};
    wxLongLong_t        m_startMs {0};    // wall-clock start of animation

    // Labels for transient per-frame HUD objects in the DDS.
    // Each frame the old objects are removed by label before new ones are added,
    // keeping the DDS at a constant size rather than accumulating indefinitely.
    static constexpr const char *kLabelHudText = "hud_text";
    static constexpr const char *kLabelHudBar  = "hud_bar";

    // ---- camera ----
    void setupCamera()
    {
        wxbgi_cam_create("orbitcam", WXBGI_CAM_PERSPECTIVE);
        wxbgi_cam_set_perspective("orbitcam", 50.0f, 0.1f, 200.0f);
        wxbgi_cam_set_up("orbitcam", 0.0f, 0.0f, 1.0f);
        wxbgi_cam_set_screen_viewport("orbitcam", 0, 0, m_canvasW, m_canvasH);
        wxbgi_cam_set_active("orbitcam");
        applyOrbitAtFrame(0);
    }

    void applyOrbitAtFrame(int frame)
    {
        float t  = (kTotalFrames <= 1) ? 0.0f
                                       : (float)frame / (float)(kTotalFrames - 1);
        float ar = t * 2.0f * (float)M_PI;
        float er = kElevDeg * (float)M_PI / 180.0f;
        float ex = kDist * cosf(er) * cosf(ar);
        float ey = kDist * cosf(er) * sinf(ar);
        float ez = kDist * sinf(er);
        wxbgi_cam_set_eye   ("orbitcam", ex, ey, ez);
        wxbgi_cam_set_target("orbitcam", 0.0f, 0.0f, 0.5f);
        wxbgi_cam_set_active("orbitcam");
    }

    // ---- scene (built once in wireframe mode) ----
    void buildScene()
    {
        wxbgi_solid_set_draw_mode(WXBGI_SOLID_WIREFRAME);

        setcolor(DARKGRAY);
        for (int i = -5; i <= 5; ++i) {
            wxbgi_world_line((float)i, -5.0f, 0.0f, (float)i, 5.0f, 0.0f);
            wxbgi_world_line(-5.0f, (float)i, 0.0f, 5.0f, (float)i, 0.0f);
        }
        setcolor(RED);   wxbgi_world_line(0,0,0, 3,0,0);
        setcolor(GREEN); wxbgi_world_line(0,0,0, 0,3,0);
        setcolor(BLUE);  wxbgi_world_line(0,0,0, 0,0,3);

        wxbgi_solid_set_edge_color(WHITE);

        wxbgi_solid_set_face_color(RED);
        wxbgi_solid_box(0.0f, 0.0f, 0.75f, 1.5f, 1.5f, 1.5f);

        wxbgi_solid_set_face_color(CYAN);
        wxbgi_solid_sphere(3.5f, 0.0f, 1.0f, 1.0f, 10, 8);   // 160 tris

        wxbgi_solid_set_face_color(YELLOW);
        wxbgi_solid_cylinder(0.0f, 3.5f, 0.0f, 0.7f, 2.0f, 10, 1);

        wxbgi_solid_set_face_color(MAGENTA);
        wxbgi_solid_cone(-3.0f, -3.0f, 0.0f, 0.8f, 2.2f, 10, 1);

        wxbgi_solid_set_face_color(LIGHTGREEN);
        wxbgi_solid_torus(-3.5f, 0.0f, 1.0f, 0.9f, 0.35f, 10, 6); // 120 tris
    }

    // ---- per-frame render ----
    void renderFrame(int frame)
    {
        applyOrbitAtFrame(frame);
        cleardevice();
        wxbgi_render_dds("orbitcam");

        // Remove previous frame's HUD objects from the DDS so they don't
        // accumulate across frames (transient DDS object pattern).
        const char *prevText = wxbgi_dds_find_by_label(kLabelHudText);
        if (prevText && prevText[0]) wxbgi_dds_remove(prevText);
        const char *prevBar  = wxbgi_dds_find_by_label(kLabelHudBar);
        if (prevBar  && prevBar[0])  wxbgi_dds_remove(prevBar);

        // Draw HUD text label.
        float azimDeg = (kTotalFrames <= 1) ? 0.0f
                      : (float)frame / (float)(kTotalFrames - 1) * 360.0f;
        char buf[128];
        setcolor(WHITE);
        snprintf(buf, sizeof(buf),
                 "Orbit test  frame %d/%d  azim=%.0f  %s",
                 frame + 1, kTotalFrames, azimDeg,
                 m_solid ? "SOLID" : "WIREFRAME");
        outtextxy(8, 8, buf);
        // Label the just-added DDS text so we can find it next frame.
        int lastIdx = wxbgi_dds_object_count() - 1;
        wxbgi_dds_set_label(wxbgi_dds_get_id_at(lastIdx), kLabelHudText);

        // Draw HUD progress bar.
        setcolor(m_solid ? YELLOW : CYAN);
        int barW = (int)((float)m_canvasW * frame / (kTotalFrames - 1));
        bar(0, m_canvasH - 6, std::max(1, barW), m_canvasH - 1);
        lastIdx = wxbgi_dds_object_count() - 1;
        wxbgi_dds_set_label(wxbgi_dds_get_id_at(lastIdx), kLabelHudBar);
    }

    // ---- timer: time-based frame advancement ----
    void OnTimer(wxTimerEvent &)
    {
        wxLongLong_t now  = wxGetLocalTimeMillis().GetValue();
        wxLongLong_t elap = now - m_startMs;

        // Compute logical frame from elapsed time (clamp to last frame).
        int frame = (int)((double)elap / (double)kDurationMs * (kTotalFrames - 1));
        frame = std::min(frame, kTotalFrames - 1);

        // Switch to smooth-shaded Phong mode at half-way.
        if (!m_solid && frame >= kHalfFrames) {
            m_solid = true;
            wxbgi_dds_set_solid_draw_mode(WXBGI_SOLID_SMOOTH);
        }

        renderFrame(frame);
        m_canvas->Refresh(false);

        // Done when elapsed time >= total duration.
        if (elap >= kDurationMs) {
            m_timer.Stop();
            CallAfter([this]() {
                printf("wx_bgi_3d_orbit_test: orbit complete.\n");
                fflush(stdout);
                Close();
            });
        }
    }

    // ---- first-paint deferred init ----
    void OnFirstPaint(wxPaintEvent &evt)
    {
        evt.Skip();
        if (!m_ready) {
            m_ready = true;
            m_canvas->Unbind(wxEVT_PAINT, &OrbitFrame::OnFirstPaint, this);
            CallAfter([this]() {
                wxSize sz = m_canvas->GetClientSize();
                m_canvasW = std::max(1, sz.GetWidth());
                m_canvasH = std::max(1, sz.GetHeight());

                setupCamera();
                buildScene();

                renderFrame(0);
                m_canvas->Refresh(false);

                m_startMs = wxGetLocalTimeMillis().GetValue();
                Bind(wxEVT_TIMER, &OrbitFrame::OnTimer, this);
                m_timer.Start(kTimerMs);
            });
        }
    }

    void OnClose(wxCloseEvent&)
    {
        // wxWidgets/GLX teardown crashes on Linux when the GL context is
        // destroyed after the X11 surface is gone. _Exit() terminates the
        // process immediately without running atexit handlers or destructors,
        // avoiding the crash. Matching the approach used by bgi_wx_standalone.
        _Exit(0);
    }

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(OrbitFrame, wxFrame)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
class OrbitTestApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        (new OrbitFrame)->Show();
        return true;
    }
    int OnExit() override { return 0; }
};

wxIMPLEMENT_APP(OrbitTestApp);
