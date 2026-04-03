// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "wx_bgi_ext.h"

#include "bgi_draw.h"
#include "bgi_overlay.h"
#include "bgi_state.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace
{
    bool ensureReadyUnlocked()
    {
        // In wx-embedded mode the window is managed by wxWidgets, not GLFW.
        if (bgi::gState.wxEmbedded)
        {
            bgi::gState.lastResult = bgi::grOk;
            return true;
        }
        if (bgi::gState.window == nullptr)
        {
            bgi::gState.lastResult = bgi::grNoInitGraph;
            return false;
        }

        if (glfwWindowShouldClose(bgi::gState.window) != 0)
        {
            bgi::gState.lastResult = bgi::grWindowClosed;
            return false;
        }

        return true;
    }

    float clampColor(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    int checkedByteCount(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            return -1;
        }

        constexpr int kChannels = 4;
        const long long bytes = static_cast<long long>(width) * static_cast<long long>(height) * kChannels;
        if (bytes <= 0 || bytes > static_cast<long long>(INT32_MAX))
        {
            return -1;
        }

        return static_cast<int>(bytes);
    }
} // namespace

BGI_API int BGI_CALL wxbgi_is_ready(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return ensureReadyUnlocked() ? 1 : 0;
}

BGI_API int BGI_CALL wxbgi_poll_events(void)
{
    void (*wxCb)() = nullptr;
    {
        std::lock_guard<std::mutex> lock(bgi::gMutex);
        if (!ensureReadyUnlocked())
        {
            return -1;
        }

        if (bgi::gState.wxEmbedded)
        {
            // Grab the callback pointer then release the lock before calling
            // it — wx event handlers (OnPaint, key events, etc.) need to
            // re-acquire gMutex and would deadlock if we held it here.
            wxCb = bgi::gState.wxPollCallback;
        }
        else
        {
            glfwPollEvents();
            bgi::gState.lastResult = bgi::grOk;
            return 0;
        }
    }
    if (wxCb) wxCb();
    return 0;
}

BGI_API int BGI_CALL wxbgi_key_pressed(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return 0;
    }

    bgi::gState.lastResult = bgi::grOk;
    return bgi::gState.keyQueue.empty() ? 0 : 1;
}

BGI_API int BGI_CALL wxbgi_read_key(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.keyQueue.empty())
    {
        bgi::gState.lastResult = bgi::grOk;
        return -1;
    }

    const int keyCode = bgi::gState.keyQueue.front();
    bgi::gState.keyQueue.pop();
    bgi::gState.lastResult = bgi::grOk;
    return keyCode;
}

BGI_API int BGI_CALL wxbgi_is_key_down(int key)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || key < 0 || key >= static_cast<int>(bgi::gState.keyDown.size()))
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    bgi::gState.lastResult = bgi::grOk;
    return bgi::gState.keyDown[static_cast<std::size_t>(key)] != 0U ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_get_mouse_pos(int *x, int *y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (x) *x = bgi::gState.mouseX;
    if (y) *y = bgi::gState.mouseY;
}

BGI_API int BGI_CALL wxbgi_mouse_moved(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const bool moved = bgi::gState.mouseMoved;
    bgi::gState.mouseMoved = false;
    return moved ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_set_key_hook(WxbgiKeyHook cb)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userKeyHook = cb;
}

BGI_API void BGI_CALL wxbgi_set_char_hook(WxbgiCharHook cb)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userCharHook = cb;
}

BGI_API void BGI_CALL wxbgi_set_cursor_pos_hook(WxbgiCursorPosHook cb)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userCursorPosHook = cb;
}

BGI_API void BGI_CALL wxbgi_set_mouse_button_hook(WxbgiMouseButtonHook cb)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userMouseButtonHook = cb;
}

BGI_API void BGI_CALL wxbgi_set_scroll_hook(WxbgiScrollHook cb)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userScrollHook = cb;
}

BGI_API void BGI_CALL wxbgi_get_scroll_delta(double *dx, double *dy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (dx) *dx = bgi::gState.scrollDeltaX;
    if (dy) *dy = bgi::gState.scrollDeltaY;
    bgi::gState.scrollDeltaX = 0.0;
    bgi::gState.scrollDeltaY = 0.0;
}

BGI_API void BGI_CALL wxbgi_set_input_defaults(int flags)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.inputDefaultFlags = flags;
}

BGI_API int BGI_CALL wxbgi_get_input_defaults(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.inputDefaultFlags;
}

// ---- Hook-context DDS functions (no mutex acquire; caller holds gMutex) ----

BGI_API int BGI_CALL wxbgi_hk_get_mouse_x(void)
{
    return bgi::gState.mouseX;
}

BGI_API int BGI_CALL wxbgi_hk_get_mouse_y(void)
{
    return bgi::gState.mouseY;
}

BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_count(void)
{
    return static_cast<int>(bgi::gState.selectedObjectIds.size());
}

BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_id(int index, char *outId, int maxLen)
{
    if (index < 0 || index >= static_cast<int>(bgi::gState.selectedObjectIds.size()))
        return -1;
    const std::string &id = bgi::gState.selectedObjectIds[static_cast<std::size_t>(index)];
    if (outId && maxLen > 0)
    {
        const std::size_t n = static_cast<std::size_t>(maxLen - 1);
        id.copy(outId, n);
        outId[std::min(n, id.size())] = '\0';
    }
    return static_cast<int>(id.size());
}

BGI_API int BGI_CALL wxbgi_hk_dds_is_selected(const char *id)
{
    if (!id) return 0;
    const std::string sid(id);
    for (const auto &s : bgi::gState.selectedObjectIds)
        if (s == sid) return 1;
    return 0;
}

BGI_API void BGI_CALL wxbgi_hk_dds_select(const char *id)
{
    if (!id) return;
    const std::string sid(id);
    for (const auto &s : bgi::gState.selectedObjectIds)
        if (s == sid) return;
    bgi::gState.selectedObjectIds.push_back(sid);
}

BGI_API void BGI_CALL wxbgi_hk_dds_deselect(const char *id)
{
    if (!id) return;
    const std::string sid(id);
    auto &v = bgi::gState.selectedObjectIds;
    v.erase(std::remove(v.begin(), v.end(), sid), v.end());
}

BGI_API void BGI_CALL wxbgi_hk_dds_deselect_all(void)
{
    bgi::gState.selectedObjectIds.clear();
}

BGI_API int BGI_CALL wxbgi_hk_dds_pick_at(int x, int y, int ctrl)
{
    bgi::overlayPerformPick(x, y, ctrl != 0);
    return static_cast<int>(bgi::gState.selectedObjectIds.size());
}

#ifdef WXBGI_ENABLE_TEST_SEAMS
BGI_API int BGI_CALL wxbgi_test_clear_key_queue(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    while (!bgi::gState.keyQueue.empty())
    {
        bgi::gState.keyQueue.pop();
    }
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_inject_key_code(int keyCode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || keyCode < 0 || keyCode > 255)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    bgi::gState.keyQueue.push(keyCode);
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_inject_extended_scan(int scanCode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || scanCode < 0 || scanCode > 255)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    bgi::gState.keyQueue.push(0);
    bgi::gState.keyQueue.push(scanCode);
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_key(int key, int scancode, int action, int mods)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    // Replicate keyCallback: update keyDown for all action types.
    if (key >= 0 && key < static_cast<int>(bgi::gState.keyDown.size()))
    {
        bgi::gState.keyDown[static_cast<std::size_t>(key)] =
            (action != WXBGI_KEY_RELEASE) ? std::uint8_t{1U} : std::uint8_t{0U};
    }

    // On release: call hook then return (mirrors keyCallback early-return path).
    if (action != WXBGI_KEY_PRESS && action != WXBGI_KEY_REPEAT)
    {
        if (bgi::gState.userKeyHook)
            bgi::gState.userKeyHook(key, scancode, action, mods);
        bgi::gState.lastResult = bgi::grOk;
        return 0;
    }

    // Press/repeat: translate special keys into keyQueue entries (mirrors keyCallback switch).
    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
    {
        auto pushKey = [](int k) { bgi::gState.keyQueue.push(k); };
        auto pushExt = [&pushKey](int scan) { pushKey(0); pushKey(scan); };

        switch (key)
        {
        case GLFW_KEY_ESCAPE:                pushKey(27);   break;
        case GLFW_KEY_ENTER:  /* fall-through */
        case GLFW_KEY_KP_ENTER:              pushKey(13);   break;
        case GLFW_KEY_TAB:                   pushKey(9);    break;
        case GLFW_KEY_BACKSPACE:             pushKey(8);    break;
        case GLFW_KEY_UP:                    pushExt(72);   break;
        case GLFW_KEY_DOWN:                  pushExt(80);   break;
        case GLFW_KEY_LEFT:                  pushExt(75);   break;
        case GLFW_KEY_RIGHT:                 pushExt(77);   break;
        case GLFW_KEY_HOME:                  pushExt(71);   break;
        case GLFW_KEY_END:                   pushExt(79);   break;
        case GLFW_KEY_PAGE_UP:               pushExt(73);   break;
        case GLFW_KEY_PAGE_DOWN:             pushExt(81);   break;
        case GLFW_KEY_INSERT:                pushExt(82);   break;
        case GLFW_KEY_DELETE:                pushExt(83);   break;
        case GLFW_KEY_F1:                    pushExt(59);   break;
        case GLFW_KEY_F2:                    pushExt(60);   break;
        case GLFW_KEY_F3:                    pushExt(61);   break;
        case GLFW_KEY_F4:                    pushExt(62);   break;
        case GLFW_KEY_F5:                    pushExt(63);   break;
        case GLFW_KEY_F6:                    pushExt(64);   break;
        case GLFW_KEY_F7:                    pushExt(65);   break;
        case GLFW_KEY_F8:                    pushExt(66);   break;
        case GLFW_KEY_F9:                    pushExt(67);   break;
        case GLFW_KEY_F10:                   pushExt(68);   break;
        case GLFW_KEY_F11:                   pushExt(133);  break;
        case GLFW_KEY_F12:                   pushExt(134);  break;
        default: break; // Printable keys: charCallback handles them in real usage.
        }
    }

    if (bgi::gState.userKeyHook)
        bgi::gState.userKeyHook(key, scancode, action, mods);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_char(unsigned int codepoint)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    // Replicate charCallback filter exactly (including early return for
    // out-of-range and control chars — hook NOT called for those).
    if (codepoint <= 0U || codepoint > 255U)
    {
        bgi::gState.lastResult = bgi::grOk;
        return 0;
    }
    if (codepoint == 9U || codepoint == 13U || codepoint == 27U)
    {
        bgi::gState.lastResult = bgi::grOk;
        return 0;
    }

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
        bgi::gState.keyQueue.push(static_cast<int>(codepoint));

    if (bgi::gState.userCharHook)
        bgi::gState.userCharHook(codepoint);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_cursor(int x, int y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_CURSOR_TRACK)
    {
        bgi::gState.mouseX     = x;
        bgi::gState.mouseY     = y;
        bgi::gState.mouseMoved = true;
    }

    if (bgi::gState.userCursorPosHook)
        bgi::gState.userCursorPosHook(x, y);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_mouse_button(int button, int action, int mods)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    // overlayPerformPick is intentionally omitted (requires rendered framebuffer).
    if (bgi::gState.userMouseButtonHook)
        bgi::gState.userMouseButtonHook(button, action, mods);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_scroll(double xoffset, double yoffset)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_SCROLL_ACCUM)
    {
        bgi::gState.scrollDeltaX += xoffset;
        bgi::gState.scrollDeltaY += yoffset;
    }

    if (bgi::gState.userScrollHook)
        bgi::gState.userScrollHook(xoffset, yoffset);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}
#endif

BGI_API int BGI_CALL wxbgi_should_close(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.wxEmbedded)
        return bgi::gState.shouldClose ? 1 : 0;
    if (bgi::gState.window == nullptr)
    {
        bgi::gState.lastResult = bgi::grNoInitGraph;
        return 1;
    }

    return glfwWindowShouldClose(bgi::gState.window) != 0 ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_request_close(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.wxEmbedded)
    {
        bgi::gState.shouldClose = true;
        bgi::gState.lastResult  = bgi::grOk;
        return;
    }
    if (bgi::gState.window == nullptr)
    {
        bgi::gState.lastResult = bgi::grNoInitGraph;
        return;
    }

    glfwSetWindowShouldClose(bgi::gState.window, GLFW_TRUE);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API int BGI_CALL wxbgi_make_context_current(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (!bgi::gState.wxEmbedded)
        glfwMakeContextCurrent(bgi::gState.window);
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_swap_window_buffers(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (!bgi::gState.wxEmbedded)
        glfwSwapBuffers(bgi::gState.window);
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_set_vsync(int enabled)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (!bgi::gState.wxEmbedded)
    {
        glfwMakeContextCurrent(bgi::gState.window);
        glfwSwapInterval(enabled != 0 ? 1 : 0);
    }
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_get_window_size(int *width, int *height)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || width == nullptr || height == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    if (bgi::gState.wxEmbedded)
    {
        *width  = bgi::gState.width;
        *height = bgi::gState.height;
    }
    else
    {
        glfwGetWindowSize(bgi::gState.window, width, height);
    }
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_get_framebuffer_size(int *width, int *height)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || width == nullptr || height == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    if (bgi::gState.wxEmbedded)
    {
        *width  = bgi::gState.width;
        *height = bgi::gState.height;
    }
    else
    {
        glfwGetFramebufferSize(bgi::gState.window, width, height);
    }
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_set_window_title(const char *title)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || title == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    bgi::gState.windowTitle = title;
    if (!bgi::gState.wxEmbedded)
        glfwSetWindowTitle(bgi::gState.window, bgi::gState.windowTitle.c_str());
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API double BGI_CALL wxbgi_get_time_seconds(void)
{
    if (bgi::gState.wxEmbedded)
    {
        static const auto s_start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - s_start).count();
    }
    return glfwGetTime();
}

BGI_API void *BGI_CALL wxbgi_get_proc_address(const char *procName)
{
    if (procName == nullptr)
        return nullptr;

#ifdef _WIN32
    if (bgi::gState.wxEmbedded)
    {
        // In wx mode the context is a wgl context — use wglGetProcAddress directly.
        PROC p = wglGetProcAddress(procName);
        if (p != nullptr)
            return reinterpret_cast<void *>(p);
        // Fall back to the GL module (for legacy entry points like glClear)
        HMODULE hgl = GetModuleHandleA("opengl32.dll");
        return hgl ? reinterpret_cast<void *>(GetProcAddress(hgl, procName)) : nullptr;
    }
#endif
    return reinterpret_cast<void *>(glfwGetProcAddress(procName));
}

BGI_API const char *BGI_CALL wxbgi_get_gl_string(int which)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
        return "";

    // Static placeholder strings for wx mode when context is not yet current.
    static const char* kWxPlaceholders[4] = {
        "wx-mode", "wx-mode", "wx-mode (GL context pending)", "wx-mode"
    };

    if (!bgi::gState.wxEmbedded)
        glfwMakeContextCurrent(bgi::gState.window);

    GLenum name = GL_VENDOR;
    switch (which)
    {
    case 0: name = GL_VENDOR;                  break;
    case 1: name = GL_RENDERER;                break;
    case 2: name = GL_VERSION;                 break;
    case 3: name = GL_SHADING_LANGUAGE_VERSION; break;
    default:
        bgi::gState.lastResult = bgi::grInvalidInput;
        return "";
    }

    const GLubyte *value = glGetString(name);
    if (!value && bgi::gState.wxEmbedded && which >= 0 && which < 4)
    {
        // GL context not yet active — return a safe placeholder.
        bgi::gState.lastResult = bgi::grOk;
        return kWxPlaceholders[which];
    }
    bgi::gState.lastResult = value != nullptr ? bgi::grOk : bgi::grError;
    return value != nullptr ? reinterpret_cast<const char *>(value) : "";
}

BGI_API int BGI_CALL wxbgi_begin_advanced_frame(float r, float g, float b, float a, int clearColor, int clearDepth)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.wxEmbedded)
    {
        // In wx mode, the canvas manages GL context + swap. Just clear the BGI
        // page buffer when clearColor is requested.
        if (clearColor != 0)
            bgi::clearActivePage(bgi::gState.bkColor);
        bgi::gState.lastResult = bgi::grOk;
        return 0;
    }

    glfwMakeContextCurrent(bgi::gState.window);
    glfwPollEvents();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(bgi::gState.window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    GLbitfield flags = 0;
    if (clearColor != 0)
    {
        glClearColor(clampColor(r), clampColor(g), clampColor(b), clampColor(a));
        flags |= GL_COLOR_BUFFER_BIT;
    }
    if (clearDepth != 0)
    {
        glClearDepth(1.0);
        flags |= GL_DEPTH_BUFFER_BIT;
    }

    if (flags != 0)
    {
        glClear(flags);
    }

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_end_advanced_frame(int swapBuffers)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.wxEmbedded)
    {
        // In wx mode, the canvas handles flush + swap on next paint event.
        bgi::gState.lastResult = bgi::grOk;
        return 0;
    }

    glFlush();
    if (swapBuffers != 0)
    {
        glfwSwapBuffers(bgi::gState.window);
    }

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_read_pixels_rgba8(int x, int y, int width, int height, unsigned char *outBuffer, int outBufferSize)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || outBuffer == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    const int byteCount = checkedByteCount(width, height);
    if (byteCount < 0 || outBufferSize < byteCount)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    if (!bgi::gState.wxEmbedded)
        glfwMakeContextCurrent(bgi::gState.window);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outBuffer);

    bgi::gState.lastResult = bgi::grOk;
    return byteCount;
}

BGI_API int BGI_CALL wxbgi_write_pixels_rgba8(int x, int y, int width, int height, const unsigned char *inBuffer, int inBufferSize)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || inBuffer == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    const int byteCount = checkedByteCount(width, height);
    if (byteCount < 0 || inBufferSize < byteCount)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    int framebufferWidth  = bgi::gState.width;
    int framebufferHeight = bgi::gState.height;
    if (!bgi::gState.wxEmbedded)
        glfwGetFramebufferSize(bgi::gState.window, &framebufferWidth, &framebufferHeight);

    const long long x2 = static_cast<long long>(x) + static_cast<long long>(width);
    const long long y2 = static_cast<long long>(y) + static_cast<long long>(height);
    if (x < 0 || y < 0 || x2 > framebufferWidth || y2 > framebufferHeight)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    if (!bgi::gState.wxEmbedded)
        glfwMakeContextCurrent(bgi::gState.window);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glWindowPos2i(x, y);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, inBuffer);

    bgi::gState.lastResult = bgi::grOk;
    return byteCount;
}

// =============================================================================
// Extended RGB palette API  (wxbgi_define_color / wxbgi_alloc_color / ...)
// =============================================================================

BGI_API int BGI_CALL wxbgi_define_color(int idx, int r, int g, int b)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (idx < bgi::kExtColorBase || idx >= bgi::kExtColorBase + bgi::kExtPaletteSize)
        return -1;
    const std::size_t slot = static_cast<std::size_t>(idx - bgi::kExtColorBase);
    bgi::gState.extPalette[slot] = {bgi::normalizeColorByte(r),
                                    bgi::normalizeColorByte(g),
                                    bgi::normalizeColorByte(b)};
    return idx;
}

BGI_API int BGI_CALL wxbgi_alloc_color(int r, int g, int b)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.extColorNext >= bgi::kExtColorBase + bgi::kExtPaletteSize)
        return -1;
    const int idx = bgi::gState.extColorNext++;
    const std::size_t slot = static_cast<std::size_t>(idx - bgi::kExtColorBase);
    bgi::gState.extPalette[slot] = {bgi::normalizeColorByte(r),
                                    bgi::normalizeColorByte(g),
                                    bgi::normalizeColorByte(b)};
    return idx;
}

BGI_API void BGI_CALL wxbgi_free_color(int idx)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (idx < bgi::kExtColorBase || idx >= bgi::kExtColorBase + bgi::kExtPaletteSize)
        return;
    const std::size_t slot = static_cast<std::size_t>(idx - bgi::kExtColorBase);
    bgi::gState.extPalette[slot] = {0, 0, 0};
    // Step the alloc pointer back if this was the most recently allocated slot.
    if (idx + 1 == bgi::gState.extColorNext)
        --bgi::gState.extColorNext;
}

BGI_API void BGI_CALL wxbgi_getrgb(int color, int *r, int *g, int *b)
{
    if (!r || !g || !b)
        return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const bgi::ColorRGB rgb = bgi::colorToRGB(color);
    *r = rgb.r;
    *g = rgb.g;
    *b = rgb.b;
}

// ---------------------------------------------------------------------------
// wxWidgets embedding helpers
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_wx_init_for_canvas(int width, int height)
{
    bgi::initForWxCanvas(width, height);
}

/// Internal: called by bgi_wx_standalone.cpp to register a wx event pump.
/// The callback is invoked by wxbgi_poll_events() WITHOUT holding gMutex.
BGI_API void BGI_CALL wxbgi_wx_set_poll_callback(void (*fn)())
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.wxPollCallback = fn;
}

BGI_API void BGI_CALL wxbgi_wx_render_page_gl(int width, int height)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::renderPageToCurrentGLContext(width, height);
}

BGI_API void BGI_CALL wxbgi_wx_resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::resetStateForWindow(width, height, true);
}

BGI_API void BGI_CALL wxbgi_wx_get_size(int* width, int* height)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (width)  *width  = bgi::gState.width;
    if (height) *height = bgi::gState.height;
}

BGI_API void BGI_CALL wxbgi_wx_key_event(int glfwKey, int action)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (glfwKey >= 0 && glfwKey < static_cast<int>(bgi::gState.keyDown.size()))
        bgi::gState.keyDown[static_cast<std::size_t>(glfwKey)] =
            (action == WXBGI_KEY_PRESS) ? 1U : 0U;

    if (bgi::gState.userKeyHook)
        bgi::gState.userKeyHook(glfwKey, glfwKey, action, 0);

    if (action != WXBGI_KEY_PRESS) return;

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
    {
        auto pushKey = [](int k) { bgi::gState.keyQueue.push(k); };
        auto pushExt = [&pushKey](int sc) { pushKey(0); pushKey(sc); };
        switch (glfwKey)
        {
        case GLFW_KEY_ESCAPE:                        pushKey(27);  break;
        case GLFW_KEY_ENTER:  /* fall-through */
        case GLFW_KEY_KP_ENTER:                      pushKey(13);  break;
        case GLFW_KEY_TAB:                           pushKey(9);   break;
        case GLFW_KEY_BACKSPACE:                     pushKey(8);   break;
        case GLFW_KEY_UP:                            pushExt(72);  break;
        case GLFW_KEY_DOWN:                          pushExt(80);  break;
        case GLFW_KEY_LEFT:                          pushExt(75);  break;
        case GLFW_KEY_RIGHT:                         pushExt(77);  break;
        case GLFW_KEY_HOME:                          pushExt(71);  break;
        case GLFW_KEY_END:                           pushExt(79);  break;
        case GLFW_KEY_PAGE_UP:                       pushExt(73);  break;
        case GLFW_KEY_PAGE_DOWN:                     pushExt(81);  break;
        case GLFW_KEY_INSERT:                        pushExt(82);  break;
        case GLFW_KEY_DELETE:                        pushExt(83);  break;
        case GLFW_KEY_F1:                            pushExt(59);  break;
        case GLFW_KEY_F2:                            pushExt(60);  break;
        case GLFW_KEY_F3:                            pushExt(61);  break;
        case GLFW_KEY_F4:                            pushExt(62);  break;
        case GLFW_KEY_F5:                            pushExt(63);  break;
        case GLFW_KEY_F6:                            pushExt(64);  break;
        case GLFW_KEY_F7:                            pushExt(65);  break;
        case GLFW_KEY_F8:                            pushExt(66);  break;
        case GLFW_KEY_F9:                            pushExt(67);  break;
        case GLFW_KEY_F10:                           pushExt(68);  break;
        case GLFW_KEY_F11:                           pushExt(133); break;
        case GLFW_KEY_F12:                           pushExt(134); break;
        default: break;
        }
    }
}

BGI_API void BGI_CALL wxbgi_wx_char_event(int codepoint)
{
    if (codepoint <= 0 || codepoint > 255 ||
        codepoint == 9 || codepoint == 13 || codepoint == 27) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
        bgi::gState.keyQueue.push(codepoint);
    if (bgi::gState.userCharHook)
        bgi::gState.userCharHook(static_cast<unsigned int>(codepoint));
}

BGI_API void BGI_CALL wxbgi_wx_mouse_move(int x, int y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_CURSOR_TRACK)
    {
        bgi::gState.mouseX     = x;
        bgi::gState.mouseY     = y;
        bgi::gState.mouseMoved = true;
    }
    if (bgi::gState.userCursorPosHook)
        bgi::gState.userCursorPosHook(x, y);
}

BGI_API void BGI_CALL wxbgi_wx_mouse_button(int btn, int action)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if ((bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_MOUSE_PICK) &&
        btn == WXBGI_MOUSE_LEFT && action == WXBGI_KEY_PRESS)
    {
        bgi::overlayPerformPick(bgi::gState.mouseX, bgi::gState.mouseY, false);
    }
    if (bgi::gState.userMouseButtonHook)
        bgi::gState.userMouseButtonHook(btn, action, 0);
}

BGI_API void BGI_CALL wxbgi_wx_scroll(double xDelta, double yDelta)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_SCROLL_ACCUM)
        bgi::gState.scrollDeltaY += yDelta;
    if (bgi::gState.userScrollHook)
        bgi::gState.userScrollHook(xDelta, yDelta);
}