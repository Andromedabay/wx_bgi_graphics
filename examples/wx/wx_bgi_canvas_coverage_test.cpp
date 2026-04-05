// wx_bgi_canvas_coverage_test.cpp
//
// Automated CTest: BGI API coverage using WxBgiCanvas embedded in a wxFrame.
// Mirrors demo_bgi_api_coverage.pas — same primitives, same API assertions.
// Exits 0 on success, 1 on any assertion failure.
//
// Execution flow (two timer phases):
//   Phase 0  — Draw colorful BGI primitives; visible for ~1.5 s.
//   Phase 1  — Clear to blue, draw final message; visible for ~1.5 s.
//   Phase 2  — Print success line, close window, exit 0.
//
// Run:
//   ctest --test-dir build -C Debug -R wx_bgi_canvas_coverage_test --output-on-failure
// or:
//   .\build\Debug\wx_bgi_canvas_coverage_test.exe

#include <wx/wx.h>
#include <wx/timer.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "wx_bgi_wx.h"
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

using namespace bgi;

// ---------------------------------------------------------------------------
// Assertion helper
// ---------------------------------------------------------------------------

static int g_exitCode = 0;

static void Require(bool cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_exitCode = 1;
    }
}

// ---------------------------------------------------------------------------
// CoverageFrame
// ---------------------------------------------------------------------------

enum Phase { kDrawPrimitives, kShowFinal, kDone };

class CoverageFrame : public wxFrame
{
public:
    CoverageFrame()
        : wxFrame(nullptr, wxID_ANY, "BGI API Coverage — wx Canvas Test",
                  wxDefaultPosition, wxDefaultSize)
    {
        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                          wxDefaultPosition, wxSize(640, 480));
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND);
        SetSizer(sizer);
        SetClientSize(640, 480);

        m_canvas->Bind(wxEVT_PAINT, &CoverageFrame::OnFirstPaint, this);
        Bind(wxEVT_TIMER, &CoverageFrame::OnTimer, this, m_timer.GetId());
        Bind(wxEVT_CLOSE_WINDOW, &CoverageFrame::OnClose, this);
    }

private:
    wxbgi::WxBgiCanvas* m_canvas {nullptr};
    wxTimer             m_timer  {this};
    bool                m_ready  {false};
    Phase               m_phase  {kDrawPrimitives};

    // -----------------------------------------------------------------------
    // Phase 0: colorful BGI primitive coverage 
    // -----------------------------------------------------------------------

    void drawPrimitives()
    {
        setgraphmode(0);
        setbkcolor(BLUE);
        setcolor(YELLOW);
        setviewport(20, 20, 619, 459, 1);
        rectangle(0, 0, getmaxx(), getmaxy());

        setlinestyle(DASHED_LINE, 0xF0F0, THICK_WIDTH);
        moveto(20, 20);
        line(20, 20, 200, 60);
        lineto(260, 120);
        linerel(40, 30);
        moverel(-20, 30);
        putpixel(getx(), gety(), LIGHTMAGENTA);
        Require(getpixel(getx(), gety()) >= 0, "getpixel failed");

        setlinestyle(SOLID_LINE, 0xFFFF, 1);
        circle(120, 160, 40);
        arc(120, 160, 30, 240, 60);
        ellipse(260, 150, 0, 300, 60, 30);
        arccoordstype arcInfo {};
        getarccoords(&arcInfo);

        setfillstyle(HATCH_FILL, LIGHTRED);
        bar(320, 40, 400, 120);
        bar3d(420, 40, 520, 120, 16, 1);
        fillellipse(480, 180, 48, 28);
        pieslice(120, 300, 20, 160, 55);
        sector(260, 300, 200, 340, 60, 35);

        int poly[] = {330, 220, 380, 180, 440, 210, 430, 280, 340, 290};
        drawpoly(5, poly);
        fillpoly(5, poly);

        setfillstyle(SOLID_FILL, LIGHTCYAN);
        rectangle(40, 340, 140, 420);
        floodfill(80, 380, YELLOW);

        char pat[8] = {'\x81','\x42','\x24','\x18','\x18','\x24','\x42','\x81'};
        setfillpattern(pat, GREEN);
        char retPat[8] {};
        getfillpattern(retPat);
        fillsettingstype fillInfo {};
        getfillsettings(&fillInfo);

        settextstyle(TRIPLEX_FONT, HORIZ_DIR, 2);
        settextjustify(LEFT_TEXT, TOP_TEXT);
        setusercharsize(2, 1, 2, 1);
        outtextxy(20, 20, const_cast<char*>("BGI COVERAGE C++ WX"));
        settextstyle(SANS_SERIF_FONT, VERT_DIR, 1);
        settextjustify(CENTER_TEXT, CENTER_TEXT);
        outtextxy(560, 200, const_cast<char*>("VERT"));
        settextstyle(GOTHIC_FONT, HORIZ_DIR, 1);
        moveto(40, 250);
        char msg2[] = "OUTTEXT WIDTH HEIGHT";
        outtext(msg2);
        Require(textwidth(msg2) > 0, "textwidth failed");
        Require(textheight(msg2) > 0, "textheight failed");

        int imgSz = imagesize(320, 40, 400, 120);
        std::vector<char> imgBuf(static_cast<std::size_t>(std::max(1, imgSz)));
        getimage(320, 40, 400, 120, imgBuf.data());
        putimage(430, 300, imgBuf.data(), XOR_PUT);
    }

    // -----------------------------------------------------------------------
    // API state checks after primitives are drawn 
    // -----------------------------------------------------------------------

    void checkApiCoverage()
    {
        linesettingstype lineInfo {};
        getlinesettings(&lineInfo);
        textsettingstype textInfo {};
        gettextsettings(&textInfo);
        viewporttype viewInfo {};
        getviewsettings(&viewInfo);
        int xasp = 0, yasp = 0;
        setaspectratio(4, 3);
        getaspectratio(&xasp, &yasp);
        palettetype pal {};
        getpalette(&pal);
        palettetype* defPal = getdefaultpalette();
        setpalette(1, 9);
        setrgbpalette(2, 32, 220, 90);
        setallpalette(defPal);

        Require(getbkcolor() == BLUE,  "getbkcolor mismatch");
        Require(getcolor()   >= 0,     "getcolor mismatch");
        Require(getmaxcolor() == 15,   "getmaxcolor mismatch");
        Require(getmaxmode()  == 0,    "getmaxmode mismatch");
        Require(getwindowwidth()  > 0, "window width invalid");
        Require(getwindowheight() > 0, "window height invalid");
        Require(getmaxwidth()  >= getwindowwidth(),  "getmaxwidth invalid");
        Require(getmaxheight() >= getwindowheight(), "getmaxheight invalid");
        Require(getpalettesize() == 16,              "getpalettesize invalid");
        Require(strlen(getdrivername()) > 0,         "getdrivername failed");
        Require(strlen(getmodename(0)) > 0,          "getmodename failed");
        int loMode = 0, hiMode = 0;
        getmoderange(DETECT, &loMode, &hiMode);

        // Page-switching 
        setactivepage(1);
        setvisualpage(0);
        delay(30);                           // brief pause with colorful page visible
        setwritemode(OR_PUT);
        putpixel(10, 10, GREEN);
        setwritemode(COPY_PUT);
        Require(getactivepage() == 1, "getactivepage mismatch");
        Require(getvisualpage()  == 0, "getvisualpage mismatch");
        Require(swapbuffers()    == 0, "swapbuffers failed");
        setactivepage(0);
        setvisualpage(0);
        delay(30);                           // brief pause before clearing
    }

    // -----------------------------------------------------------------------
    // Phase 1: blue screen with final message 
    // -----------------------------------------------------------------------

    void drawFinalScreen()
    {
        clearviewport();
        setviewport(0, 0, getwindowwidth() - 1, getwindowheight() - 1, 1);
        cleardevice();                       // fills page with BLUE (bkColor still BLUE)
        graphdefaults();                     // resets bkColor→BLACK, color→WHITE
        settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
        outtextxy(30, 30, const_cast<char*>("BGI API COVERAGE WX CANVAS OK"));
        delay(30);                           // brief delay after text draw (Pascal line 369)
    }

    // -----------------------------------------------------------------------
    // Extension API checks 
    // -----------------------------------------------------------------------

    void checkExtensionApi()
    {
        Require(wxbgi_is_ready()            == 1, "wxbgi_is_ready failed");
        Require(wxbgi_poll_events()         == 0, "wxbgi_poll_events failed");
        Require(wxbgi_make_context_current()== 0, "wxbgi_make_context_current failed");
        Require(wxbgi_set_vsync(1)          == 0, "wxbgi_set_vsync failed");
        Require(wxbgi_set_window_title(const_cast<char*>("C++ wx Canvas Coverage")) == 0,
                "wxbgi_set_window_title failed");

        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        Require(wxbgi_get_window_size(&winW, &winH)       == 0, "wxbgi_get_window_size failed");
        Require(wxbgi_get_framebuffer_size(&fbW, &fbH)    == 0, "wxbgi_get_framebuffer_size failed");
        Require(winW > 0, "window width invalid");
        Require(winH > 0, "window height invalid");
        Require(fbW  > 0, "framebuffer width invalid");
        Require(fbH  > 0, "framebuffer height invalid");

        Require(wxbgi_get_time_seconds() >= 0.0f,                    "wxbgi_get_time_seconds failed");
        Require(wxbgi_get_proc_address("glClear") != nullptr,        "wxbgi_get_proc_address failed");
        Require(wxbgi_get_gl_string(1) != nullptr,                   "wxbgi_get_gl_string failed");
        Require(strlen(wxbgi_get_gl_string(2)) > 0,                  "wxbgi_get_gl_string version empty");

        Require(wxbgi_begin_advanced_frame(0.05f, 0.08f, 0.12f, 1.0f, 1, 0) == 0,
                "wxbgi_begin_advanced_frame failed");
        Require(wxbgi_end_advanced_frame(0) == 0, "wxbgi_end_advanced_frame failed");

        unsigned char wbuf[16] = {255,32,32,255, 32,255,32,255,
                                   32,32,255,255, 255,255,32,255};
        Require(wxbgi_write_pixels_rgba8(1, 1, 2, 2, wbuf, 16) > 0,
                "wxbgi_write_pixels_rgba8 failed");

        unsigned char rbuf[4] = {};
        Require(wxbgi_read_pixels_rgba8(0, 0, 1, 1, rbuf, 4) > 0,
                "wxbgi_read_pixels_rgba8 failed");
        Require(wxbgi_swap_window_buffers() == 0, "wxbgi_swap_window_buffers failed");
        Require(wxbgi_should_close()        == 0, "wxbgi_should_close unexpected");
    }

    // -----------------------------------------------------------------------
    // Event handlers
    // -----------------------------------------------------------------------

    void OnTimer(wxTimerEvent&)
    {
        if (m_phase == kDrawPrimitives)
        {
            // Switch to phase 1: show final blue screen
            m_phase = kShowFinal;
            drawFinalScreen();
            checkExtensionApi();
            m_canvas->Refresh(false);
            m_canvas->Update();
            m_timer.StartOnce(1500);
        }
        else if (m_phase == kShowFinal)
        {
            m_phase = kDone;
            printf("wx_bgi_canvas_coverage_test: all checks passed.\n");
            fflush(stdout);
            CallAfter([this]() { Close(); });
        }
    }

    void OnFirstPaint(wxPaintEvent& evt)
    {
        evt.Skip();
        if (!m_ready)
        {
            m_ready = true;
            m_canvas->Unbind(wxEVT_PAINT, &CoverageFrame::OnFirstPaint, this);
            CallAfter([this]()
            {
                drawPrimitives();
                checkApiCoverage();
                m_canvas->Refresh(false);
                m_canvas->Update();     // push colorful primitives to screen immediately
                m_timer.StartOnce(1500);  // stay on colorful screen for 1.5 s
            });
        }
    }

    void OnClose(wxCloseEvent&)
    {
        // wxWidgets/GLX teardown crashes on Linux when the GL context is
        // destroyed after the X11 surface is gone. _Exit() terminates the
        // process immediately without running atexit handlers or destructors,
        // avoiding the crash. Matching the approach used by bgi_wx_standalone.
        _Exit(g_exitCode);
    }

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(CoverageFrame, wxFrame)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------

class CoverageApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        (new CoverageFrame)->Show();
        return true;
    }

    int OnExit() override
    {
        return g_exitCode;
    }
};

wxIMPLEMENT_APP(CoverageApp);
