// wx_bgi_canvas.cpp — WxBgiCanvas implementation.
// Uses only the exported C API from wx_bgi_ext.h — no internal bgi:: symbols.
// Compiled into wx_bgi_wx (STATIC lib) so it shares the EXE's wxWidgets copy.
#include <GL/glew.h>
#include "wx_bgi_canvas.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include <wx/dcclient.h>

// GLFW key-code aliases used for key mapping.
#ifndef GLFW_KEY_SPACE
#  define GLFW_KEY_SPACE       32
#  define GLFW_KEY_ESCAPE     256
#  define GLFW_KEY_ENTER      257
#  define GLFW_KEY_TAB        258
#  define GLFW_KEY_BACKSPACE  259
#  define GLFW_KEY_RIGHT      262
#  define GLFW_KEY_LEFT       263
#  define GLFW_KEY_DOWN       264
#  define GLFW_KEY_UP         265
#  define GLFW_KEY_F1         290
#  define GLFW_KEY_F12        301
#  define GLFW_KEY_KP_ENTER   335
#  define GLFW_PRESS            1
#  define GLFW_RELEASE          0
#endif

namespace wxbgi {

// Keep pixel-format attributes minimal: don't request a specific GL version here.
// Requesting WX_GL_MAJOR_VERSION sets needsARB=true in wxWidgets which causes
// the entire context creation to fail if wglCreateContextAttribsARB returns NULL
// on any driver.  Instead we let the driver provide its best compat context, then
// check GLEW post-init for GL 3.3 features and fall back to legacy if needed.
//
// WX_GL_DEPTH_SIZE 24 is required for correct hidden-surface removal: without an
// explicit depth-buffer attachment the depth test trivially passes for all
// fragments (draw order wins), producing completely wrong depth ordering for 3-D
// solid and wireframe rendering.
static const int kGLAttrs[] = {
    WX_GL_RGBA,
    WX_GL_DOUBLEBUFFER,
    WX_GL_DEPTH_SIZE, 24,
    0
};

wxBEGIN_EVENT_TABLE(WxBgiCanvas, wxGLCanvas)
    EVT_PAINT(WxBgiCanvas::OnPaint)
    EVT_SIZE(WxBgiCanvas::OnSize)
    EVT_KEY_DOWN(WxBgiCanvas::OnKeyDown)
    EVT_KEY_UP(WxBgiCanvas::OnKeyUp)
    EVT_CHAR(WxBgiCanvas::OnChar)
    EVT_MOTION(WxBgiCanvas::OnMouseMove)
    EVT_LEFT_DOWN(WxBgiCanvas::OnMouseDown)
    EVT_LEFT_UP(WxBgiCanvas::OnMouseUp)
    EVT_RIGHT_DOWN(WxBgiCanvas::OnMouseDown)
    EVT_RIGHT_UP(WxBgiCanvas::OnMouseUp)
    EVT_MIDDLE_DOWN(WxBgiCanvas::OnMouseDown)
    EVT_MIDDLE_UP(WxBgiCanvas::OnMouseUp)
    EVT_MOUSEWHEEL(WxBgiCanvas::OnMouseWheel)
wxEND_EVENT_TABLE()

WxBgiCanvas::WxBgiCanvas(wxWindow* parent, wxWindowID id,
                         const wxPoint& pos, const wxSize& size,
                         long style, const wxString& name)
    // wxFULL_REPAINT_ON_RESIZE ensures correct viewport after resize.
    : wxGLCanvas(parent, id, kGLAttrs, pos, size,
                 style | wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE, name)
{
    // NOTE: No GL work here.  wxGLContext is created lazily on the first paint
    // (following the official wxWidgets OpenGL sample pattern) so that a valid
    // context is always current before any GL call is made.
}

WxBgiCanvas::~WxBgiCanvas()
{
    Unbind(wxEVT_IDLE, &WxBgiCanvas::OnIdle, this);
    if (m_glContext && m_glewInited)
    {
        // Make context current so GL cleanup calls are valid, then release
        // all GL objects (textures, VAOs, programs) before deleting the context.
        // Skipping this causes driver crashes on some Windows GPU drivers during
        // process shutdown (STATUS_FATAL_APP_EXIT / 0xc000041d).
        SetCurrent(*m_glContext);
        wxbgi_gl_pass_destroy();
    }
    delete m_glContext;
}

void WxBgiCanvas::SetAutoRefreshHz(int hz)
{
    m_autoRefreshHz = hz;
    if (hz > 0)
        Bind(wxEVT_IDLE, &WxBgiCanvas::OnIdle, this);
    else
        Unbind(wxEVT_IDLE, &WxBgiCanvas::OnIdle, this);
}

void WxBgiCanvas::Render()
{
    if (!IsShownOnScreen()) return;

    // Lazily create the GL context on the first render — official wx pattern.
    if (!m_glContext)
        m_glContext = new wxGLContext(this);

    if (!m_glContext->IsOK())
        return;

    SetCurrent(*m_glContext);

    // One-time GLEW + BGI initialisation, executed with a valid GL context.
    if (!m_glewInited)
    {
        glewExperimental = GL_TRUE;
        glewInit();
        m_glewInited = true;

        // If the context doesn't support GL 3.3 (VAOs, shaders), fall back to
        // the legacy GL_POINTS rendering path which only requires GL 1.x.
        if (!GLEW_VERSION_3_3)
            wxbgi_set_legacy_gl_render(1);
    }
    if (!m_bgiInited)
    {
        // Only call wxbgi_wx_init_for_canvas if BGI has not already been
        // initialized externally (e.g. by wxbgi_wx_frame_create).
        if (!wxbgi_is_ready())
        {
            const wxSize sz = GetClientSize();
            wxbgi_wx_init_for_canvas(std::max(1, sz.GetWidth()),
                                     std::max(1, sz.GetHeight()));
        }
        m_bgiInited = true;
        SetFocus();
    }

    int pageW = 0, pageH = 0;
    wxbgi_wx_get_size(&pageW, &pageH);
    if (pageW > 0 && pageH > 0)
    {
        // Compute physical pixel size for glViewport — on high-DPI displays
        // the framebuffer is larger than the logical (DIP) canvas size.
        const wxSize logSz    = GetClientSize();
        const double dpiScale = GetDPIScaleFactor();
        const int vpW = std::max(pageW, (int)(logSz.GetWidth()  * dpiScale));
        const int vpH = std::max(pageH, (int)(logSz.GetHeight() * dpiScale));
        wxbgi_wx_render_page_gl_vp(pageW, pageH, vpW, vpH);
    }

    SwapBuffers();
}

void WxBgiCanvas::OnPaint(wxPaintEvent& /*evt*/)
{
    wxPaintDC dc(this);
    Render();
}

void WxBgiCanvas::OnSize(wxSizeEvent& evt)
{
    if (m_bgiInited)
    {
        const wxSize sz = evt.GetSize();
        if (sz.GetWidth() > 0 && sz.GetHeight() > 0)
        {
            // Only resize the BGI pixel buffer if the size actually changed.
            // On Windows, multiple WM_SIZE events fire for the same size during
            // Show() + sizer layout, which would unnecessarily clear the page.
            int curW = 0, curH = 0;
            wxbgi_wx_get_size(&curW, &curH);
            if (sz.GetWidth() != curW || sz.GetHeight() != curH)
                wxbgi_wx_resize(sz.GetWidth(), sz.GetHeight());
        }
    }
    Refresh();
    evt.Skip();
}

void WxBgiCanvas::OnIdle(wxIdleEvent& evt)
{
    if (m_autoRefreshHz <= 0) return;
    const wxLongLong_t nowMs   = wxGetLocalTimeMillis().GetValue();
    const wxLongLong_t frameMs = 1000 / m_autoRefreshHz;
    if (nowMs - m_lastRenderMs >= frameMs)
    {
        m_lastRenderMs = nowMs;
        Refresh(false);
    }
    evt.RequestMore();
}

int WxBgiCanvas::WxKeyToGlfw(int wxKey)
{
    switch (wxKey)
    {
    case WXK_ESCAPE:       return GLFW_KEY_ESCAPE;
    case WXK_RETURN:       return GLFW_KEY_ENTER;
    case WXK_NUMPAD_ENTER: return GLFW_KEY_KP_ENTER;
    case WXK_TAB:          return GLFW_KEY_TAB;
    case WXK_BACK:         return GLFW_KEY_BACKSPACE;
    case WXK_LEFT:         return GLFW_KEY_LEFT;
    case WXK_RIGHT:        return GLFW_KEY_RIGHT;
    case WXK_UP:           return GLFW_KEY_UP;
    case WXK_DOWN:         return GLFW_KEY_DOWN;
    case WXK_F1:           return GLFW_KEY_F1;
    case WXK_F2:           return GLFW_KEY_F1 + 1;
    case WXK_F3:           return GLFW_KEY_F1 + 2;
    case WXK_F4:           return GLFW_KEY_F1 + 3;
    case WXK_F5:           return GLFW_KEY_F1 + 4;
    case WXK_F6:           return GLFW_KEY_F1 + 5;
    case WXK_F7:           return GLFW_KEY_F1 + 6;
    case WXK_F8:           return GLFW_KEY_F1 + 7;
    case WXK_F9:           return GLFW_KEY_F1 + 8;
    case WXK_F10:          return GLFW_KEY_F1 + 9;
    case WXK_F11:          return GLFW_KEY_F1 + 10;
    case WXK_F12:          return GLFW_KEY_F12;
    case WXK_HOME:         return 268;
    case WXK_END:          return 269;
    case WXK_PAGEUP:       return 266;
    case WXK_PAGEDOWN:     return 267;
    case WXK_INSERT:       return 260;
    case WXK_DELETE:       return 261;
    default:
        if (wxKey >= 32 && wxKey < 512) return wxKey;
        return -1;
    }
}

void WxBgiCanvas::RouteKeyEvent(wxKeyEvent& evt, int action)
{
    const int glfwKey = WxKeyToGlfw(evt.GetKeyCode());
    if (glfwKey < 0) { evt.Skip(); return; }
    wxbgi_wx_key_event(glfwKey, action);
    if (action != GLFW_PRESS) evt.Skip();
}

void WxBgiCanvas::OnKeyDown(wxKeyEvent& evt) { RouteKeyEvent(evt, GLFW_PRESS); }
void WxBgiCanvas::OnKeyUp(wxKeyEvent& evt)   { RouteKeyEvent(evt, GLFW_RELEASE); }

void WxBgiCanvas::OnChar(wxKeyEvent& evt)
{
    const int cp = static_cast<int>(evt.GetUnicodeKey());
    wxbgi_wx_char_event(cp);
    evt.Skip();
}

void WxBgiCanvas::OnMouseMove(wxMouseEvent& evt)
{
    wxbgi_wx_mouse_move(evt.GetX(), evt.GetY());
    evt.Skip();
}

void WxBgiCanvas::OnMouseDown(wxMouseEvent& evt)
{
    int btn = -1;
    if (evt.LeftDown())        btn = WXBGI_MOUSE_LEFT;
    else if (evt.RightDown())  btn = WXBGI_MOUSE_RIGHT;
    else if (evt.MiddleDown()) btn = WXBGI_MOUSE_MIDDLE;
    if (btn >= 0) wxbgi_wx_mouse_button(btn, WXBGI_KEY_PRESS);
    evt.Skip();
}

void WxBgiCanvas::OnMouseUp(wxMouseEvent& evt)
{
    int btn = -1;
    if (evt.LeftUp())        btn = WXBGI_MOUSE_LEFT;
    else if (evt.RightUp())  btn = WXBGI_MOUSE_RIGHT;
    else if (evt.MiddleUp()) btn = WXBGI_MOUSE_MIDDLE;
    if (btn >= 0) wxbgi_wx_mouse_button(btn, WXBGI_KEY_RELEASE);
    evt.Skip();
}

void WxBgiCanvas::OnMouseWheel(wxMouseEvent& evt)
{
    const double delta = static_cast<double>(evt.GetWheelRotation()) /
                         static_cast<double>(evt.GetWheelDelta());
    wxbgi_wx_scroll(0.0, delta);
    evt.Skip();
}

} // namespace wxbgi
