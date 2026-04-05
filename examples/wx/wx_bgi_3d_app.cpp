// wx_bgi_3d_app.cpp — interactive 3-D demo: orbiting camera + solid primitives
//
// Controls
//   Arrow keys        Orbit the camera (azimuth / elevation)
//   +  /  -           Zoom in / out
//   W                 Cycle render mode: Wireframe → Flat → Smooth (Phong)
//   Mouse left-drag   Orbit (azimuth × elevation)
//   Mouse scroll      Zoom
//   R                 Reset camera to default position
//   Escape / Alt-F4   Quit

#include <wx/wx.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
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

// ── Camera state (spherical coordinates, Z-up) ───────────────────────────────

struct OrbitCam {
    float azimDeg{30.0f};   // horizontal angle around Z axis
    float elevDeg{25.0f};   // vertical angle above XY plane
    float dist{14.0f};      // distance from target
    float tx{0.0f}, ty{0.0f}, tz{0.5f}; // look-at target

    static constexpr float kMinElev =  5.0f;
    static constexpr float kMaxElev = 85.0f;
    static constexpr float kMinDist =  2.0f;
    static constexpr float kMaxDist = 60.0f;

    void clamp() {
        elevDeg = std::max(kMinElev, std::min(kMaxElev, elevDeg));
        dist    = std::max(kMinDist, std::min(kMaxDist, dist));
    }

    void eye(float& ex, float& ey, float& ez) const {
        float ar = azimDeg * (float)M_PI / 180.0f;
        float er = elevDeg * (float)M_PI / 180.0f;
        ex = tx + dist * cosf(er) * cosf(ar);
        ey = ty + dist * cosf(er) * sinf(ar);
        ez = tz + dist * sinf(er);
    }
};

// ── Frame ─────────────────────────────────────────────────────────────────────

class Bgi3DFrame : public wxFrame
{
public:
    Bgi3DFrame()
        : wxFrame(nullptr, wxID_ANY, "wx_bgi 3-D Demo",
                  wxDefaultPosition, wxSize(900, 720))
    {
        CreateStatusBar(2);
        updateStatus();

        auto* menu = new wxMenuBar;
        auto* fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT, "E&xit\tAlt-F4");
        menu->Append(fileMenu, "&File");
        SetMenuBar(menu);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                          wxDefaultPosition, wxSize(840, 640));

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND | wxALL, 4);
        SetSizer(sizer);

        // Defer GL/BGI setup until first paint.
        m_canvas->Bind(wxEVT_PAINT, &Bgi3DFrame::OnFirstPaint, this);

        // ── input bindings ────────────────────────────────────────────────

        m_canvas->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
            if (!m_ready) { e.Skip(); return; }
            bool redraw = true;
            switch (e.GetKeyCode()) {
            case WXK_LEFT:       m_cam.azimDeg -= 5.0f;  break;
            case WXK_RIGHT:      m_cam.azimDeg += 5.0f;  break;
            case WXK_UP:         m_cam.elevDeg += 5.0f;  break;
            case WXK_DOWN:       m_cam.elevDeg -= 5.0f;  break;
            case '+': case '=':  m_cam.dist    -= 1.0f;  break;
            case '-': case '_':  m_cam.dist    += 1.0f;  break;
            case 'W': case 'w':
                // Cycle: Wireframe(0) → Flat(1) → Smooth(2) → Wireframe(0)
                m_drawMode = (m_drawMode + 1) % 3;
                // Rebuild scene so DDS reflects new draw mode.
                buildScene();
                break;
            case 'R': case 'r':
                m_cam = OrbitCam{};
                break;
            default: redraw = false; e.Skip(); break;
            }
            if (redraw) { m_cam.clamp(); renderScene(); m_canvas->Render(); }
            updateStatus();
        });

        // Left-drag to orbit.
        m_canvas->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
            m_dragging   = true;
            m_dragPt     = e.GetPosition();
            m_dragAzim0  = m_cam.azimDeg;
            m_dragElev0  = m_cam.elevDeg;
            m_canvas->CaptureMouse();
            e.Skip();
        });
        m_canvas->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
            if (m_dragging) {
                m_dragging = false;
                if (m_canvas->HasCapture()) m_canvas->ReleaseMouse();
            }
            e.Skip();
        });
        m_canvas->Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
            if (m_ready && m_dragging) {
                wxPoint delta = e.GetPosition() - m_dragPt;
                m_cam.azimDeg = m_dragAzim0 - delta.x * 0.4f;
                m_cam.elevDeg = m_dragElev0 + delta.y * 0.3f;
                m_cam.clamp();
                renderScene();
                m_canvas->Render();
                updateStatus();
            }
            e.Skip();
        });

        // Scroll to zoom.
        m_canvas->Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent& e) {
            if (!m_ready) { e.Skip(); return; }
            float factor = (e.GetWheelRotation() > 0) ? 0.9f : 1.1f;
            m_cam.dist *= factor;
            m_cam.clamp();
            renderScene();
            m_canvas->Render();
            updateStatus();
            e.Skip();
        });
    }

private:
    wxbgi::WxBgiCanvas* m_canvas{nullptr};
    bool      m_ready{false};
    int       m_drawMode{WXBGI_SOLID_WIREFRAME}; // 0=wire, 1=flat, 2=smooth
    OrbitCam  m_cam;
    bool      m_dragging{false};
    wxPoint   m_dragPt;
    float     m_dragAzim0{0}, m_dragElev0{0};
    int       m_canvasW{840}, m_canvasH{640};

    // ── BGI / DDS setup ───────────────────────────────────────────────────

    void setupCamera()
    {
        // Only create once.
        static bool created = false;
        if (!created) {
            wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
            created = true;
        }
        wxbgi_cam_set_perspective("cam3d", 50.0f, 0.1f, 200.0f);
        wxbgi_cam_set_up("cam3d", 0.0f, 0.0f, 1.0f);
        wxbgi_cam_set_screen_viewport("cam3d", 0, 0, m_canvasW, m_canvasH);
        wxbgi_cam_set_active("cam3d");
        applyOrbit();
    }

    void applyOrbit()
    {
        float ex, ey, ez;
        m_cam.eye(ex, ey, ez);
        wxbgi_cam_set_eye("cam3d", ex, ey, ez);
        wxbgi_cam_set_target("cam3d", m_cam.tx, m_cam.ty, m_cam.tz);
    }

    // Build the static scene into the DDS (called once, or on mode change).
    void buildScene()
    {
        wxbgi_dds_clear();
        cleardevice();

        int mode = m_drawMode;
        wxbgi_solid_set_draw_mode(mode);

        // Set up Phong lighting (used by FLAT and SMOOTH modes).
        // Key light from upper-left-front; warm fill from below.
        wxbgi_solid_set_light_dir(-0.577f, 0.577f, 0.577f);
        wxbgi_solid_set_light_space(1);  // world-space
        wxbgi_solid_set_fill_light(0.5f, -0.5f, -0.5f, 0.25f);
        wxbgi_solid_set_ambient(0.20f);
        wxbgi_solid_set_diffuse(0.70f);
        wxbgi_solid_set_specular(0.40f, 48.f);

        // Ground grid (world lines in XY plane)
        setcolor(DARKGRAY);
        for (int i = -5; i <= 5; ++i) {
            wxbgi_world_line((float)i, -5.0f, 0.0f, (float)i, 5.0f, 0.0f);
            wxbgi_world_line(-5.0f, (float)i, 0.0f, 5.0f, (float)i, 0.0f);
        }

        // Axes (X=red, Y=green, Z=blue)
        setcolor(RED);   wxbgi_world_line(0,0,0, 3,0,0);
        setcolor(GREEN); wxbgi_world_line(0,0,0, 0,3,0);
        setcolor(BLUE);  wxbgi_world_line(0,0,0, 0,0,3);

        // Solid primitives
        wxbgi_solid_set_edge_color(WHITE);

        wxbgi_solid_set_face_color(RED);
        wxbgi_solid_box(0.0f, 0.0f, 0.75f, 1.5f, 1.5f, 1.5f);

        wxbgi_solid_set_face_color(CYAN);
        wxbgi_solid_sphere(3.5f, 0.0f, 1.0f, 1.0f, 28, 18);

        wxbgi_solid_set_face_color(YELLOW);
        wxbgi_solid_cylinder(0.0f, 3.5f, 0.0f, 0.7f, 2.0f, 24, 1);

        wxbgi_solid_set_face_color(MAGENTA);
        wxbgi_solid_cone(-3.0f, -3.0f, 0.0f, 0.8f, 2.2f, 20, 1);

        wxbgi_solid_set_face_color(LIGHTGREEN);
        wxbgi_solid_torus(-3.5f, 0.0f, 1.0f, 0.9f, 0.35f, 24, 12);
    }

    // Re-render existing DDS with updated camera.
    void renderScene()
    {
        applyOrbit();
        cleardevice();
        wxbgi_render_dds("cam3d");

        // Overlay HUD (pixel-space BGI text, drawn AFTER render_dds)
        setcolor(WHITE);
        char buf[128];
        const char* modeStr = (m_drawMode == WXBGI_SOLID_WIREFRAME) ? "Wireframe" :
                              (m_drawMode == WXBGI_SOLID_FLAT)      ? "Flat"      : "Smooth";
        snprintf(buf, sizeof(buf),
                 "Azim %.0f  Elev %.0f  Dist %.1f  [%s]",
                 m_cam.azimDeg, m_cam.elevDeg, m_cam.dist, modeStr);
        outtextxy(8, 8, buf);
        setcolor(LIGHTGRAY);
        outtextxy(8, m_canvasH - 22, const_cast<char*>(
            "Arrows:orbit  +/-:zoom  W:cycle-mode  R:reset  Drag:orbit  Scroll:zoom"));
    }

    // ── first-paint deferred init ─────────────────────────────────────────

    void OnFirstPaint(wxPaintEvent& evt)
    {
        evt.Skip();
        if (!m_ready) {
            m_ready = true;
            m_canvas->Unbind(wxEVT_PAINT, &Bgi3DFrame::OnFirstPaint, this);
            CallAfter([this]() {
                wxSize sz = m_canvas->GetClientSize();
                m_canvasW = std::max(1, sz.GetWidth());
                m_canvasH = std::max(1, sz.GetHeight());
                setupCamera();
                buildScene();
                renderScene();
                m_canvas->SetAutoRefreshHz(0);  // repaint on demand
                m_canvas->Render();
            });
        }
    }

    // ── status bar ───────────────────────────────────────────────────────

    void updateStatus()
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Azim=%.0f°  Elev=%.0f°  Dist=%.1f",
                 m_cam.azimDeg, m_cam.elevDeg, m_cam.dist);
        SetStatusText(buf, 0);
        SetStatusText(m_drawMode == WXBGI_SOLID_WIREFRAME ? "Wireframe" :
                      m_drawMode == WXBGI_SOLID_FLAT      ? "Flat"      : "Smooth", 1);
    }
};

// ── app ───────────────────────────────────────────────────────────────────────

class Bgi3DApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        (new Bgi3DFrame)->Show();
        return true;
    }
};

wxIMPLEMENT_APP(Bgi3DApp);
