/**
 * @file wx_multi_scene_demo.cpp
 * @brief Multi-CHDOP / Multi-Scene demo hosted in a native wxWidgets Frame.
 *
 * Duplicates the logic of wxbgi_multi_scene_demo.cpp but wraps it in a proper
 * wxFrame with a menu bar and status bar.
 *
 * Layout (3-panel canvas):
 *
 *   ┌───────────────┬────────────────┐
 *   │  cam_a        │  cam_b         │
 *   │  "main" scene │  "main" scene  │
 *   ├───────────────┴────────────────┤
 *   │  cam_c  (bottom half)          │
 *   │  "secondary" scene (bar chart) │
 *   └────────────────────────────────┘
 *
 * Menu bar:
 *   File      : Quit
 *   Camera A  : Smooth / Flat / Wireframe (radio), Reset Yaw
 *   Camera B  : Smooth / Flat / Wireframe (radio), Reset Pitch
 *   Help      : Controls, About
 *
 * Status bar  : [cam_a: yaw=XX°  Smooth]  [cam_b: pitch=XX°  Flat]
 *
 * Keyboard shortcuts (canvas focus):
 *   Left/Right   — orbit cam_a (yaw)
 *   Up/Down      — orbit cam_b (pitch)
 *   S / F / W    — cam_a shading: Smooth / Flat / Wireframe
 *   1 / 2 / 3    — cam_b shading: Smooth / Flat / Wireframe
 *   Escape       — quit
 */

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

// ---------------------------------------------------------------------------
// Canvas dimensions
// ---------------------------------------------------------------------------
static constexpr int kW = 900;
static constexpr int kH = 600;

// ---------------------------------------------------------------------------
// Menu / command IDs
// ---------------------------------------------------------------------------
enum {
    ID_CAM_A_SMOOTH = wxID_HIGHEST + 100,
    ID_CAM_A_FLAT,
    ID_CAM_A_WIRE,
    ID_CAM_A_RESET,
    ID_CAM_B_SMOOTH,
    ID_CAM_B_FLAT,
    ID_CAM_B_WIRE,
    ID_CAM_B_RESET,
    ID_CONTROLS,
};

// ---------------------------------------------------------------------------
// MultiSceneFrame
// ---------------------------------------------------------------------------
class MultiSceneFrame : public wxFrame
{
public:
    MultiSceneFrame()
        : wxFrame(nullptr, wxID_ANY, "wx_bgi Multi-Scene Demo",
                  wxDefaultPosition, wxDefaultSize)
    {
        buildMenuBar();

        // Status bar: 2 fields — cam_a info | cam_b info.
        CreateStatusBar(2);
        SetStatusText("cam_a", 0);
        SetStatusText("cam_b", 1);

        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                          wxDefaultPosition, wxSize(kW, kH));

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND);
        SetSizer(sizer);
        SetClientSize(kW, kH + 0); // let sizer add status bar height

        // Deferred GL / BGI init — run after first paint when context is ready.
        m_canvas->Bind(wxEVT_PAINT, &MultiSceneFrame::OnFirstPaint, this);

        // --- Key bindings -------------------------------------------------
        m_canvas->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
            const int k = e.GetKeyCode();
            switch (k) {
            case WXK_LEFT:   m_keyLeft  = true; break;
            case WXK_RIGHT:  m_keyRight = true; break;
            case WXK_UP:     m_keyUp    = true; break;
            case WXK_DOWN:   m_keyDown  = true; break;
            case WXK_ESCAPE: Close();           return;
            // cam_a shading
            case 'S': setCamAShading(WXBGI_SOLID_SMOOTH);     break;
            case 'F': setCamAShading(WXBGI_SOLID_FLAT);       break;
            case 'W': setCamAShading(WXBGI_SOLID_WIREFRAME);  break;
            // cam_b shading
            case '1': setCamBShading(WXBGI_SOLID_SMOOTH);     break;
            case '2': setCamBShading(WXBGI_SOLID_FLAT);       break;
            case '3': setCamBShading(WXBGI_SOLID_WIREFRAME);  break;
            default: break;
            }
            e.Skip();
        });
        m_canvas->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& e) {
            switch (e.GetKeyCode()) {
            case WXK_LEFT:  m_keyLeft  = false; break;
            case WXK_RIGHT: m_keyRight = false; break;
            case WXK_UP:    m_keyUp    = false; break;
            case WXK_DOWN:  m_keyDown  = false; break;
            default: break;
            }
            e.Skip();
        });
    }

private:
    wxbgi::WxBgiCanvas* m_canvas{nullptr};
    bool  m_ready{false};

    // camera state
    float m_camAYaw{30.f};
    float m_camBPitch{20.f};
    int   m_camAShading{WXBGI_SOLID_SMOOTH};
    int   m_camBShading{WXBGI_SOLID_SMOOTH};

    // held-key state for continuous orbit
    bool  m_keyLeft{false}, m_keyRight{false};
    bool  m_keyUp{false},   m_keyDown{false};

    // ── Menu construction ─────────────────────────────────────────────────

    void buildMenuBar()
    {
        auto* mb = new wxMenuBar;

        // File
        auto* fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT, "E&xit\tAlt-F4");
        mb->Append(fileMenu, "&File");

        // Camera A — radio items for shading
        auto* camAMenu = new wxMenu;
        camAMenu->AppendRadioItem(ID_CAM_A_SMOOTH, "&Smooth Shading\tS");
        camAMenu->AppendRadioItem(ID_CAM_A_FLAT,   "&Flat Shading\tF");
        camAMenu->AppendRadioItem(ID_CAM_A_WIRE,   "&Wireframe\tW");
        camAMenu->AppendSeparator();
        camAMenu->Append(ID_CAM_A_RESET, "&Reset Yaw");
        mb->Append(camAMenu, "Camera &A");
        m_camAMenu = camAMenu;

        // Camera B — radio items for shading
        auto* camBMenu = new wxMenu;
        camBMenu->AppendRadioItem(ID_CAM_B_SMOOTH, "&Smooth Shading\t1");
        camBMenu->AppendRadioItem(ID_CAM_B_FLAT,   "&Flat Shading\t2");
        camBMenu->AppendRadioItem(ID_CAM_B_WIRE,   "&Wireframe\t3");
        camBMenu->AppendSeparator();
        camBMenu->Append(ID_CAM_B_RESET, "&Reset Pitch");
        mb->Append(camBMenu, "Camera &B");
        m_camBMenu = camBMenu;

        // Help
        auto* helpMenu = new wxMenu;
        helpMenu->Append(ID_CONTROLS, "&Controls...\tF1");
        helpMenu->Append(wxID_ABOUT,  "&About");
        mb->Append(helpMenu, "&Help");

        SetMenuBar(mb);

        // Bind menu events
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamAShading(WXBGI_SOLID_SMOOTH); }
        }, ID_CAM_A_SMOOTH);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamAShading(WXBGI_SOLID_FLAT); }
        }, ID_CAM_A_FLAT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamAShading(WXBGI_SOLID_WIREFRAME); }
        }, ID_CAM_A_WIRE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            m_camAYaw = 30.f;
            if (m_ready) { positionCamA(); renderFrame(); m_canvas->Render(); }
        }, ID_CAM_A_RESET);

        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamBShading(WXBGI_SOLID_SMOOTH); }
        }, ID_CAM_B_SMOOTH);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamBShading(WXBGI_SOLID_FLAT); }
        }, ID_CAM_B_FLAT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_ready) { setCamBShading(WXBGI_SOLID_WIREFRAME); }
        }, ID_CAM_B_WIRE);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            m_camBPitch = 20.f;
            if (m_ready) { positionCamB(); renderFrame(); m_canvas->Render(); }
        }, ID_CAM_B_RESET);

        Bind(wxEVT_MENU, [this](wxCommandEvent&) { showControls(); }, ID_CONTROLS);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { showAbout(); }, wxID_ABOUT);
    }

    // ── Camera positioning ────────────────────────────────────────────────

    static void positionPerspCam(const char* name, float yaw, float pitch,
                                 float dist = 20.f)
    {
        const float yr = yaw   * (float)M_PI / 180.f;
        const float pr = pitch * (float)M_PI / 180.f;
        wxbgi_cam_set_eye   (name, dist * cosf(pr) * sinf(yr),
                                   dist * cosf(pr) * cosf(yr),
                                   dist * sinf(pr));
        wxbgi_cam_set_target(name, 0.f, 0.f, 0.f);
        wxbgi_cam_set_up    (name, 0.f, 0.f, 1.f);
    }

    void positionCamA() { positionPerspCam("cam_a", m_camAYaw, 20.f); }
    void positionCamB() { positionPerspCam("cam_b", -60.f, m_camBPitch); }

    // ── Scene construction ────────────────────────────────────────────────

    void buildMainScene()
    {
        wxbgi_dds_scene_set_active("main");

        setcolor(RED);       wxbgi_world_line(0,0,0, 6,0,0);
        setcolor(GREEN);     wxbgi_world_line(0,0,0, 0,6,0);
        setcolor(LIGHTBLUE); wxbgi_world_line(0,0,0, 0,0,6);

        wxbgi_solid_set_draw_mode(WXBGI_SOLID_SMOOTH);
        wxbgi_solid_set_face_color(YELLOW);
        wxbgi_solid_set_edge_color(DARKGRAY);
        wxbgi_solid_box(0.f, 0.f, 2.f, 6.f, 6.f, 4.f);

        wxbgi_solid_set_face_color(CYAN);
        wxbgi_solid_set_edge_color(DARKGRAY);
        wxbgi_solid_sphere(0.f, 0.f, 5.5f, 1.2f, 24, 16);

        wxbgi_dds_scene_set_active("default");
    }

    void buildSecondaryScene()
    {
        wxbgi_dds_scene_set_active("secondary");

        const int vpH = kH / 2;
        const int bx = 20, by = 20;

        setcolor(WHITE);
        rectangle(bx, by, kW - bx, vpH - by);

        setcolor(YELLOW);
        settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
        outtextxy(bx + 20, by + 15,
                  const_cast<char*>("cam_c: 'secondary' scene (pixel-space 2D chart)"));

        const int vals[]   = {60, 100, 45, 130, 80};
        const int bars     = 5;
        const int bar_w    = 80;
        const int bar_gap  = 20;
        const int base_y   = vpH - by - 40;
        const int start_x  = bx + 60;

        settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
        for (int i = 0; i < bars; ++i) {
            int x1 = start_x + i * (bar_w + bar_gap);
            int y2 = base_y;
            int y1 = y2 - vals[i];

            setfillstyle(SOLID_FILL, CYAN);
            bar(x1, y1, x1 + bar_w, y2);

            setcolor(WHITE);
            rectangle(x1, y1, x1 + bar_w, y2);

            char label[8];
            std::snprintf(label, sizeof(label), "%d", vals[i]);
            setcolor(YELLOW);
            outtextxy(x1 + bar_w / 2 - 8, y1 - 14, label);
        }

        setlinestyle(SOLID_LINE, 0, THICK_WIDTH);
        setcolor(WHITE);
        line(start_x - 10, base_y + 1,
             start_x + bars * (bar_w + bar_gap), base_y + 1);
        setlinestyle(SOLID_LINE, 0, NORM_WIDTH);

        const char* xlabels[] = {"A", "B", "C", "D", "E"};
        for (int i = 0; i < bars; ++i) {
            int x1 = start_x + i * (bar_w + bar_gap);
            setcolor(LIGHTGRAY);
            outtextxy(x1 + bar_w / 2 - 4, base_y + 6,
                      const_cast<char*>(xlabels[i]));
        }

        wxbgi_dds_scene_set_active("default");
    }

    // ── Shading helpers ───────────────────────────────────────────────────

    static const char* shadingLabel(int mode)
    {
        switch (mode) {
        case WXBGI_SOLID_SMOOTH:    return "Smooth";
        case WXBGI_SOLID_FLAT:      return "Flat";
        case WXBGI_SOLID_WIREFRAME: return "Wireframe";
        default:                    return "?";
        }
    }

    static void applyMainSceneShading(int mode)
    {
        wxbgi_dds_scene_set_active("main");
        wxbgi_dds_set_solid_draw_mode(mode);
        wxbgi_dds_scene_set_active("default");
    }

    void setCamAShading(int mode)
    {
        m_camAShading = mode;
        // Sync radio item check state in menu.
        m_camAMenu->FindItem(ID_CAM_A_SMOOTH)->Check(mode == WXBGI_SOLID_SMOOTH);
        m_camAMenu->FindItem(ID_CAM_A_FLAT  )->Check(mode == WXBGI_SOLID_FLAT);
        m_camAMenu->FindItem(ID_CAM_A_WIRE  )->Check(mode == WXBGI_SOLID_WIREFRAME);
        if (m_ready) { renderFrame(); m_canvas->Render(); updateStatus(); }
    }

    void setCamBShading(int mode)
    {
        m_camBShading = mode;
        m_camBMenu->FindItem(ID_CAM_B_SMOOTH)->Check(mode == WXBGI_SOLID_SMOOTH);
        m_camBMenu->FindItem(ID_CAM_B_FLAT  )->Check(mode == WXBGI_SOLID_FLAT);
        m_camBMenu->FindItem(ID_CAM_B_WIRE  )->Check(mode == WXBGI_SOLID_WIREFRAME);
        if (m_ready) { renderFrame(); m_canvas->Render(); updateStatus(); }
    }

    // ── Render ────────────────────────────────────────────────────────────

    void renderFrame()
    {
        cleardevice();

        applyMainSceneShading(m_camAShading);
        setviewport(0, 0, kW/2 - 1, kH/2 - 1, 1);
        wxbgi_render_dds("cam_a");
        setviewport(0, 0, kW - 1, kH - 1, 0);

        applyMainSceneShading(m_camBShading);
        setviewport(kW/2, 0, kW - 1, kH/2 - 1, 1);
        wxbgi_render_dds("cam_b");
        setviewport(0, 0, kW - 1, kH - 1, 0);

        setviewport(0, kH/2, kW - 1, kH - 1, 1);
        wxbgi_render_dds("cam_c");
        setviewport(0, 0, kW - 1, kH - 1, 0);

        // Panel dividers.
        setcolor(DARKGRAY);
        line(kW/2, 0,      kW/2,   kH/2 - 1);
        line(0,    kH/2,   kW - 1, kH/2);

        // Panel labels.
        setcolor(WHITE);
        settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "cam_a | ←/→ yaw=%.0f° | S/F/W: %s",
            m_camAYaw, shadingLabel(m_camAShading));
        outtextxy(4, 4, buf);
        std::snprintf(buf, sizeof(buf),
            "cam_b | ↑/↓ pitch=%.0f° | 1/2/3: %s",
            m_camBPitch, shadingLabel(m_camBShading));
        outtextxy(kW/2 + 4, 4, buf);
    }

    // ── Status bar ────────────────────────────────────────────────────────

    void updateStatus()
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "cam_a: yaw=%.0f°  %s",
                      m_camAYaw, shadingLabel(m_camAShading));
        SetStatusText(buf, 0);
        std::snprintf(buf, sizeof(buf), "cam_b: pitch=%.0f°  %s",
                      m_camBPitch, shadingLabel(m_camBShading));
        SetStatusText(buf, 1);
    }

    // ── Help dialogs ──────────────────────────────────────────────────────

    void showControls()
    {
        wxMessageBox(
            "Keyboard Controls\n"
            "------------------\n"
            "Left / Right  :  Orbit cam_a (yaw)\n"
            "Up / Down     :  Orbit cam_b (pitch)\n"
            "S             :  cam_a → Smooth shading\n"
            "F             :  cam_a → Flat shading\n"
            "W             :  cam_a → Wireframe\n"
            "1             :  cam_b → Smooth shading\n"
            "2             :  cam_b → Flat shading\n"
            "3             :  cam_b → Wireframe\n"
            "Escape        :  Quit\n"
            "\n"
            "Menu Controls\n"
            "-------------\n"
            "Camera A / B menus: shading radio items\n"
            "Camera A / B menus: Reset Yaw / Pitch",
            "Controls",
            wxOK | wxICON_INFORMATION, this);
    }

    void showAbout()
    {
        wxMessageBox(
            "wx_bgi Multi-Scene Demo\n\n"
            "Demonstrates Multi-CHDOP / Multi-Camera rendering:\n"
            "  • Two perspective cameras (cam_a, cam_b) view the same\n"
            "    'main' scene graph from different angles.\n"
            "  • One pixel-space camera (cam_c) renders a 2-D bar chart\n"
            "    from a separate 'secondary' scene graph.\n\n"
            "Built with wx_bgi_graphics + wxWidgets.",
            "About",
            wxOK | wxICON_INFORMATION, this);
    }

    // ── First-paint deferred init ─────────────────────────────────────────

    void OnFirstPaint(wxPaintEvent& evt)
    {
        evt.Skip();
        if (m_ready) return;
        m_ready = true;
        m_canvas->Unbind(wxEVT_PAINT, &MultiSceneFrame::OnFirstPaint, this);

        CallAfter([this]() {
            // Create named DDS scenes.
            wxbgi_dds_scene_create("main");
            wxbgi_dds_scene_create("secondary");

            // cam_a: left-top, perspective, "main" scene.
            wxbgi_cam_create("cam_a", WXBGI_CAM_PERSPECTIVE);
            wxbgi_cam_set_scene("cam_a", "main");
            wxbgi_cam_set_screen_viewport("cam_a", 0, 0, kW/2, kH/2);
            wxbgi_cam_set_perspective("cam_a", 45.f, 0.1f, 200.f);
            positionCamA();

            // cam_b: right-top, perspective, "main" scene.
            wxbgi_cam_create("cam_b", WXBGI_CAM_PERSPECTIVE);
            wxbgi_cam_set_scene("cam_b", "main");
            wxbgi_cam_set_screen_viewport("cam_b", kW/2, 0, kW/2, kH/2);
            wxbgi_cam_set_perspective("cam_b", 45.f, 0.1f, 200.f);
            positionCamB();

            // cam_c: bottom half, pixel-space ortho, "secondary" scene.
            wxbgi_cam_create("cam_c", WXBGI_CAM_ORTHO);
            wxbgi_cam_set_scene("cam_c", "secondary");
            wxbgi_cam_set_screen_viewport("cam_c", 0, kH/2, kW, kH/2);
            wxbgi_cam_set_ortho("cam_c",
                0.f, (float)kW, (float)(kH/2), 0.f, -1.f, 1.f);

            buildMainScene();
            buildSecondaryScene();

            // Sync initial radio check state.
            m_camAMenu->FindItem(ID_CAM_A_SMOOTH)->Check(true);
            m_camBMenu->FindItem(ID_CAM_B_SMOOTH)->Check(true);

            renderFrame();
            updateStatus();

            // 30 fps idle-driven refresh for continuous keyboard orbit.
            m_canvas->SetAutoRefreshHz(30);

            // Drive orbit + render on each idle tick.
            m_canvas->Bind(wxEVT_IDLE, &MultiSceneFrame::OnAnimIdle, this);

            m_canvas->Render();
        });
    }

    // ── Animation idle ────────────────────────────────────────────────────

    void OnAnimIdle(wxIdleEvent& evt)
    {
        if (!m_ready) return;

        bool changed = false;
        if (m_keyLeft)  { m_camAYaw -= 1.f; changed = true; }
        if (m_keyRight) { m_camAYaw += 1.f; changed = true; }
        if (m_keyUp)    { m_camBPitch = std::min(89.f, m_camBPitch + 1.f); changed = true; }
        if (m_keyDown)  { m_camBPitch = std::max(-89.f, m_camBPitch - 1.f); changed = true; }

        if (changed) {
            positionCamA();
            positionCamB();
            renderFrame();
            updateStatus();
            m_canvas->Refresh(false);
        }

        evt.RequestMore(m_keyLeft || m_keyRight || m_keyUp || m_keyDown);
    }

    // ── Member data ───────────────────────────────────────────────────────

    wxMenu* m_camAMenu{nullptr};
    wxMenu* m_camBMenu{nullptr};
};

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
class MultiSceneApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        (new MultiSceneFrame)->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MultiSceneApp);
