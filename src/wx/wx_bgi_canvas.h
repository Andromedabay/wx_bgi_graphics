#pragma once
// wx_bgi_canvas.h — WxBgiCanvas: BGI drawing surface embedded in a wxWidgets panel.
// Include this header in your application AFTER the wxWidgets headers.
//
// WxBgiCanvas is compiled into the wx_bgi_wx static library (not the DLL),
// so it uses your application''s copy of wxWidgets — no DLL-boundary issues.

#include <wx/glcanvas.h>
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

namespace wxbgi {

/**
 * @brief A wxGLCanvas that hosts the wx_bgi BGI drawing surface.
 *
 * Drop this panel into any wxFrame or wxPanel.  After the first paint the
 * BGI state is initialised and you can call any BGI drawing function.
 *
 * Typical usage:
 * @code
 *   // In wxFrame constructor — no GL work here:
 *   m_canvas = new wxbgi::WxBgiCanvas(this, wxID_ANY,
 *                                     wxDefaultPosition, wxSize(800, 600));
 *   // After first paint fires (e.g. via wxEVT_PAINT or wxCallAfter):
 *   setbkcolor(BLACK);
 *   circle(200, 200, 80);
 *   m_canvas->SetAutoRefreshHz(60);
 * @endcode
 */
class WxBgiCanvas : public wxGLCanvas
{
public:
    explicit WxBgiCanvas(wxWindow* parent,
                         wxWindowID id    = wxID_ANY,
                         const wxPoint&  pos   = wxDefaultPosition,
                         const wxSize&   size  = wxDefaultSize,
                         long            style = 0,
                         const wxString& name  = wxASCII_STR("WxBgiCanvas"));

    ~WxBgiCanvas() override;

    /** Render the BGI buffer immediately. */
    void Render();

    /**
     * @brief Enable idle-driven auto-refresh at the requested frame rate.
     * @param hz  Target refresh rate.  Pass 0 to disable.
     */
    void SetAutoRefreshHz(int hz);

protected:
    /**
     * @brief Called inside Render() after the GL context is current and BGI
     *        is initialised, but *before* the BGI pixel buffer is blitted to
     *        this canvas's OpenGL surface.
     *
     * Override in subclasses to render a specific camera's scene into the
     * shared BGI scratch buffer.  Because wx paint events are sequential on
     * the main thread, the scratch buffer is safe to reuse across panels.
     *
     * @param w  Logical pixel width  of this canvas.
     * @param h  Logical pixel height of this canvas.
     */
    virtual void PreBlit(int /*w*/, int /*h*/) {}

    /**
     * @brief Called AFTER @c wxbgi_wx_render_page_gl_vp() and all 3-D solid
     *        GL passes, but BEFORE @c SwapBuffers().
     *
     * Override in subclasses to composite overlays (concentric circles, grid,
     * UCS axes) on top of 3-D solid geometry.  Typically implemented by
     * calling @c wxbgi_wx_render_overlays_for_camera() for this panel's camera.
     *
     * @param pageW,pageH  Logical BGI page dimensions.
     * @param vpW,vpH      Physical framebuffer dimensions (may differ on HiDPI).
     */
    virtual void PostBlit(int /*pageW*/, int /*pageH*/, int /*vpW*/, int /*vpH*/) {}

private:
    wxGLContext* m_glContext{nullptr};   ///< Points to the process-wide shared context.
    bool         m_ownsGLCtx{false};    ///< True only for the first canvas that created the context.
    bool         m_glewInited{false};
    bool         m_bgiInited{false};
    int          m_autoRefreshHz{0};
    wxLongLong_t m_lastRenderMs{0};

    void OnPaint(wxPaintEvent& evt);
    void OnSize(wxSizeEvent& evt);
    void OnIdle(wxIdleEvent& evt);
    void OnKeyDown(wxKeyEvent& evt);
    void OnKeyUp(wxKeyEvent& evt);
    void OnChar(wxKeyEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);

    static int WxKeyToGlfw(int wxKey);
    void       RouteKeyEvent(wxKeyEvent& evt, int action);
};

} // namespace wxbgi
