/**
 * @file bgi_wx_standalone.cpp
 * @brief Standalone wx window API for Python, Pascal, and simple C programs.
 *
 * Implements the wxbgi_wx_app_create / wxbgi_wx_frame_create /
 * wxbgi_wx_app_main_loop family modelled on wxPython's App/Frame/MainLoop
 * pattern.  Compiled into wx_bgi_opengl.dll when WXBGI_ENABLE_WX=ON so that
 * ctypes (Python), cdecl extern (Pascal), and plain C programs can create a
 * wxWidgets window with an embedded WxBgiCanvas without writing any C++.
 *
 * NOTE: Do NOT call wxbgi_wx_app_create() from a C++ application that already
 * has wxIMPLEMENT_APP — use wx_bgi_wx.lib + WxBgiCanvas directly instead.
 */
#ifdef WXBGI_ENABLE_WX

#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/glcanvas.h>
#include "wx_bgi_canvas.h"
#include "wx_bgi_ext.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdlib>
#include <algorithm>
#include <chrono>

// ---------------------------------------------------------------------------
// Internal classes
// ---------------------------------------------------------------------------

// Forward declaration: implemented in bgi_modern_api.cpp.
extern "C" BGI_API void BGI_CALL wxbgi_wx_set_poll_callback(void(*fn)());

namespace {

WxbgiFrameCallback  g_idleCallback = nullptr;
wxbgi::WxBgiCanvas* g_canvas       = nullptr;

// Forward declarations (defined after the class)
class BgiStandaloneFrame;
static BgiStandaloneFrame* s_frame = nullptr;

// ---------------------------------------------------------------------------
// BgiStandaloneFrame — wxFrame hosting a single WxBgiCanvas.
// ---------------------------------------------------------------------------
class BgiStandaloneFrame : public wxFrame {
public:
    BgiStandaloneFrame(int w, int h, const wxString& title)
        : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxDefaultSize)
    {
        m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
                                           wxDefaultPosition, wxSize(w, h));
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_canvas, 1, wxEXPAND);
        SetSizer(sizer);
        // Make the CLIENT area (canvas) exactly w×h — not the outer window.
        SetClientSize(w, h);
        g_canvas = m_canvas;

        m_refreshTimer = new wxTimer(this, ID_REFRESH);
        m_closeTimer   = new wxTimer(this, ID_CLOSE);
        Bind(wxEVT_TIMER, &BgiStandaloneFrame::OnRefreshTimer, this, ID_REFRESH);
        Bind(wxEVT_TIMER, &BgiStandaloneFrame::OnCloseTimer,   this, ID_CLOSE);
        Bind(wxEVT_CLOSE_WINDOW, &BgiStandaloneFrame::OnClose, this);
        // Start a default 30 fps refresh timer so sequential programs (Python,
        // Pascal) that call wxbgi_wx_app_main_loop() without setting an idle
        // callback still see the current page buffer repainted periodically.
        m_refreshTimer->Start(33);
    }

    void SetFrameRate(int fps) {
        if (m_refreshTimer->IsRunning()) m_refreshTimer->Stop();
        if (fps > 0) m_refreshTimer->Start(1000 / fps);
    }

    void CloseAfterMs(int ms) {
        m_closeTimer->StartOnce(ms);
    }

private:
    enum { ID_REFRESH = wxID_HIGHEST + 1, ID_CLOSE };

    void OnRefreshTimer(wxTimerEvent&) {
        if (g_idleCallback) g_idleCallback();
        if (m_canvas) m_canvas->Refresh(false);
    }

    void OnCloseTimer(wxTimerEvent&) {
        Close(true);
    }

    void OnClose(wxCloseEvent& e) {
        m_refreshTimer->Stop();
        m_closeTimer->Stop();
        wxbgi_wx_set_poll_callback(nullptr);
        g_canvas = nullptr;
        s_frame  = nullptr;  // signal pump loop to exit
        // _Exit() terminates immediately without running atexit handlers or
        // destructors — wx/GLX teardown crashes on Linux and in DLL context.
        _Exit(0);
    }

    wxbgi::WxBgiCanvas* m_canvas        = nullptr;
    wxTimer*            m_refreshTimer  = nullptr;
    wxTimer*            m_closeTimer    = nullptr;
};

// ---------------------------------------------------------------------------
// BgiStandaloneApp — minimal wxApp subclass.
// ---------------------------------------------------------------------------
class BgiStandaloneApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

static BgiStandaloneApp*   s_app   = nullptr;
// s_frame is forward-declared above the class definition.

} // anonymous namespace

/// Callback invoked by wxbgi_poll_events() (WITHOUT gMutex held).
/// Drains the platform message queue so that keyboard/mouse events reach the
/// canvas AND the process doesn't appear "Not Responding" during synchronous
/// polling loops.  Canvas repaints are rate-limited to ~60 fps to avoid
/// hammering the GPU on every spin of "repeat until KeyPressed".
static void StandaloneYield()
{
#ifdef _WIN32
    // Drain ALL pending Win32 messages (non-blocking).  This is the same
    // mechanism as wxbgi_wx_app_main_loop but without blocking on GetMessage.
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    // Rate-limited canvas refresh (~60 fps).
    static DWORD s_lastPaint = 0;
    DWORD now = ::GetTickCount();
    if (g_canvas && (now - s_lastPaint) >= 16u) {
        s_lastPaint = now;
        g_canvas->Refresh(false);
        g_canvas->Update();
    }
#else
    // On Linux/macOS let wxWidgets dispatch pending events non-blocking.
    if (wxTheApp)
        wxTheApp->ProcessPendingEvents();
    // Rate-limited canvas refresh (~60 fps) using portable chrono.
    using Clock = std::chrono::steady_clock;
    static Clock::time_point s_lastPaint = Clock::now() - std::chrono::milliseconds(16);
    auto now = Clock::now();
    if (g_canvas &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastPaint).count() >= 16) {
        s_lastPaint = now;
        g_canvas->Refresh(false);
        g_canvas->Update();
    }
#endif
}

// ---------------------------------------------------------------------------
// Exported C API
// ---------------------------------------------------------------------------

extern "C" {

BGI_API void BGI_CALL wxbgi_wx_app_create(void)
{
    if (wxTheApp) return;  // already created (guard against double init)
    s_app = new BgiStandaloneApp();
    wxApp::SetInstance(s_app);
    int argc = 0;
    wxInitialize(argc, static_cast<wxChar**>(nullptr));
}

BGI_API void BGI_CALL wxbgi_wx_frame_create(int width, int height, const char* title)
{
    wxString wtitle = (title && *title) ? wxString::FromUTF8(title) : wxString("BGI");
    s_frame = new BgiStandaloneFrame(width, height, wtitle);
    // The constructor already called SetClientSize(width, height), so the BGI
    // pixel buffer should match exactly the requested logical size.  Calling
    // GetClientSize() here (before Show) returns physical pixels on high-DPI
    // Windows, causing a size mismatch — so we use width/height directly.
    wxbgi_wx_init_for_canvas(std::max(1, width), std::max(1, height));
    wxbgi_wx_set_poll_callback(&StandaloneYield);
    if (wxTheApp) wxTheApp->SetTopWindow(s_frame);
    s_frame->Show(true);
}

BGI_API void BGI_CALL wxbgi_wx_app_main_loop(void)
{
    if (!wxTheApp || !s_frame) {
        s_app          = nullptr;
        s_frame        = nullptr;
        g_canvas       = nullptr;
        g_idleCallback = nullptr;
        return;
    }

#ifdef _WIN32
    // Use a Win32 message pump directly; more reliable in DLL/non-wxEntry scenarios.
    MSG msg;
    while (s_frame != nullptr && ::GetMessage(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
#else
    wxTheApp->MainLoop();
#endif

    // Do NOT call wxUninitialize() here — the wx cleanup is already triggered
    // by the window destroy event, and calling it again crashes the heap.
    s_app          = nullptr;
    s_frame        = nullptr;
    g_canvas       = nullptr;
    g_idleCallback = nullptr;
}

BGI_API void BGI_CALL wxbgi_wx_close_frame(void)
{
    if (s_frame) s_frame->Close(true);
}

BGI_API void BGI_CALL wxbgi_wx_close_after_ms(int ms)
{
    if (s_frame) s_frame->CloseAfterMs(ms);
}

BGI_API void BGI_CALL wxbgi_wx_set_idle_callback(WxbgiFrameCallback fn)
{
    g_idleCallback = fn;
}

BGI_API void BGI_CALL wxbgi_wx_set_frame_rate(int fps)
{
    if (s_frame) s_frame->SetFrameRate(fps);
}

BGI_API void BGI_CALL wxbgi_wx_refresh(void)
{
    if (g_canvas) g_canvas->Refresh(false);
}

} // extern "C"

#endif // WXBGI_ENABLE_WX
