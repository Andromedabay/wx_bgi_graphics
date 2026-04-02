// wx_bgi_app.cpp — interactive 2-D demo: wx Frame + WxBgiCanvas
//
// Demonstrates live mouse tracking and key feedback using the BGI pixel API.
// Move the mouse → a magenta crosshair follows the cursor.
// Press any key  → the key name appears in the HUD.
// Press R/G/B    → toggle the triangle fill colour.
#include <wx/wx.h>
#include <cmath>
#include <cstdio>
#include "wx_bgi_wx.h"
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

using namespace bgi;

// ── frame ────────────────────────────────────────────────────────────────────

class BgiFrame : public wxFrame
{
public:
    BgiFrame()
        : wxFrame(nullptr, wxID_ANY, "wx_bgi 2-D Demo",
                  wxDefaultPosition, wxSize(860, 680))
    {
        CreateStatusBar(2);
        SetStatusText("wx_bgi embedded canvas demo");

        auto* menu = new wxMenuBar;
        auto* fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT, "E&xit\tAlt-F4");
        menu->Append(fileMenu, "&File");
        SetMenuBar(menu);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                          wxDefaultPosition, wxSize(800, 600));

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND | wxALL, 5);
        SetSizer(sizer);

        // Defer GL scene setup until after first paint (context ready).
        m_canvas->Bind(wxEVT_PAINT, &BgiFrame::OnFirstPaint, this);

        // ── input bindings ─────────────────────────────────────────────────
        // Mouse motion: update crosshair position and redraw.
        m_canvas->Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
            m_mouseX = e.GetX();
            m_mouseY = e.GetY();
            if (m_ready) { redrawScene(); m_canvas->Refresh(false); }
            e.Skip();
        });

        m_canvas->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
            if (m_ready) {
                // Build a human-readable key name
                int k = e.GetKeyCode();
                if (k >= 32 && k < 127)
                    snprintf(m_lastKey, sizeof(m_lastKey), "'%c'", (char)k);
                else {
                    const char* name = "?";
                    switch (k) {
                    case WXK_ESCAPE:  name = "Escape"; break;
                    case WXK_RETURN:  name = "Enter"; break;
                    case WXK_TAB:     name = "Tab"; break;
                    case WXK_BACK:    name = "Backspace"; break;
                    case WXK_DELETE:  name = "Delete"; break;
                    case WXK_LEFT:    name = "Left"; break;
                    case WXK_RIGHT:   name = "Right"; break;
                    case WXK_UP:      name = "Up"; break;
                    case WXK_DOWN:    name = "Down"; break;
                    case WXK_HOME:    name = "Home"; break;
                    case WXK_END:     name = "End"; break;
                    case WXK_PAGEUP:  name = "PgUp"; break;
                    case WXK_PAGEDOWN:name = "PgDn"; break;
                    case WXK_SPACE:   name = "Space"; break;
                    default:
                        if (k >= WXK_F1 && k <= WXK_F12)
                            snprintf(m_lastKey, sizeof(m_lastKey), "F%d", k - WXK_F1 + 1);
                        else
                            snprintf(m_lastKey, sizeof(m_lastKey), "#%d", k);
                        name = nullptr; break;
                    }
                    if (name) snprintf(m_lastKey, sizeof(m_lastKey), "%s", name);
                }
                // R/G/B → change triangle colour
                if (k == 'R') m_triColor = RED;
                else if (k == 'G') m_triColor = GREEN;
                else if (k == 'B') m_triColor = BLUE;
                else if (k == 'C') m_triColor = CYAN;
                else if (k == 'Y') m_triColor = YELLOW;
                else if (k == 'W') m_triColor = WHITE;
                redrawScene();
                m_canvas->Refresh(false);
            }
            e.Skip();
        });
    }

private:
    wxbgi::WxBgiCanvas* m_canvas{nullptr};
    bool  m_ready{false};
    int   m_mouseX{400}, m_mouseY{300};
    int   m_triColor{RED};
    char  m_lastKey[64]{"(none)"};

    // ── scene drawing ─────────────────────────────────────────────────────

    void redrawScene()
    {
        setbkcolor(BLACK);
        cleardevice();

        // Title
        setcolor(WHITE);
        outtextxy(10, 10, const_cast<char*>("wx_bgi 2-D Demo — move mouse, press keys"));

        // Static shapes
        setcolor(CYAN);
        circle(200, 200, 80);

        setcolor(YELLOW);
        rectangle(300, 150, 500, 300);

        // Colour-changeable triangle (press R/G/B/C/Y/W)
        setfillstyle(SOLID_FILL, m_triColor);
        int poly[] = {400, 350, 480, 450, 320, 450};
        fillpoly(3, poly);

        setcolor(GREEN);
        line(50, 400, 750, 400);

        // Mouse crosshair (follows cursor)
        setcolor(MAGENTA);
        line(m_mouseX - 18, m_mouseY, m_mouseX + 18, m_mouseY);
        line(m_mouseX, m_mouseY - 18, m_mouseX, m_mouseY + 18);
        circle(m_mouseX, m_mouseY, 7);

        // HUD
        char buf[128];
        setcolor(WHITE);
        snprintf(buf, sizeof(buf), "Mouse : (%3d, %3d)", m_mouseX, m_mouseY);
        outtextxy(10, 530, buf);
        snprintf(buf, sizeof(buf), "Last key : %s", m_lastKey);
        outtextxy(10, 548, buf);
        setcolor(LIGHTGRAY);
        outtextxy(10, 572, const_cast<char*>(
            "Move mouse | Press R G B C Y W to change triangle colour"));
    }

    // ── first-paint deferred init ─────────────────────────────────────────

    void OnFirstPaint(wxPaintEvent& evt)
    {
        evt.Skip();
        if (!m_ready) {
            m_ready = true;
            m_canvas->Unbind(wxEVT_PAINT, &BgiFrame::OnFirstPaint, this);
            CallAfter([this]() {
                redrawScene();
                m_canvas->SetAutoRefreshHz(0);   // repaint on demand only
                m_canvas->Refresh(false);
            });
        }
    }
};

// ── app ───────────────────────────────────────────────────────────────────────

class BgiApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        (new BgiFrame)->Show();
        return true;
    }
};

wxIMPLEMENT_APP(BgiApp);
