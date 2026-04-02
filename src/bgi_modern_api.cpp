// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "wx_bgi_ext.h"

#include "bgi_draw.h"
#include "bgi_overlay.h"
#include "bgi_state.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>

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
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return -1;
    }

    if (bgi::gState.wxEmbedded)
    {
        return 0;  // wx event loop handles events
    }

    glfwPollEvents();
    bgi::gState.lastResult = bgi::grOk;
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
    if (!ensureReadyUnlocked()) return -1;

    // Always update keyDown (unconditional)
    if (key >= 0 && key < static_cast<int>(bgi::gState.keyDown.size()))
        bgi::gState.keyDown[static_cast<std::size_t>(key)] = action != 0 ? 1U : 0U;

    if (bgi::gState.userKeyHook != nullptr)
        bgi::gState.userKeyHook(key, scancode, action, mods);

    if (action != 1 && action != 2) // not press or repeat
        return 0;

    if (!(bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE))
        return 0;

    // Queue key codes same as keyCallback
    auto push = [](int c) { bgi::gState.keyQueue.push(c); };
    auto pushExt = [&push](int sc) { push(0); push(sc); };

    switch (key)
    {
    case 256: push(27);   break; // GLFW_KEY_ESCAPE
    case 257: push(13);   break; // GLFW_KEY_ENTER
    case 335: push(13);   break; // GLFW_KEY_KP_ENTER
    case 258: push(9);    break; // GLFW_KEY_TAB
    case 259: push(8);    break; // GLFW_KEY_BACKSPACE
    case 265: pushExt(72); break; // GLFW_KEY_UP
    case 264: pushExt(80); break; // GLFW_KEY_DOWN
    case 263: pushExt(75); break; // GLFW_KEY_LEFT
    case 262: pushExt(77); break; // GLFW_KEY_RIGHT
    case 268: pushExt(71); break; // GLFW_KEY_HOME
    case 269: pushExt(79); break; // GLFW_KEY_END
    case 266: pushExt(73); break; // GLFW_KEY_PAGE_UP
    case 267: pushExt(81); break; // GLFW_KEY_PAGE_DOWN
    case 260: pushExt(82); break; // GLFW_KEY_INSERT
    case 261: pushExt(83); break; // GLFW_KEY_DELETE
    case 290: pushExt(59); break; // GLFW_KEY_F1
    case 291: pushExt(60); break;
    case 292: pushExt(61); break;
    case 293: pushExt(62); break;
    case 294: pushExt(63); break;
    case 295: pushExt(64); break;
    case 296: pushExt(65); break;
    case 297: pushExt(66); break;
    case 298: pushExt(67); break;
    case 299: pushExt(68); break;
    case 300: pushExt(133); break;
    case 301: pushExt(134); break; // GLFW_KEY_F12
    default:
        break;
    }

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_char(unsigned int codepoint)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked()) return -1;

    if (codepoint <= 0U || codepoint > 255U)
        return -1;
    if (codepoint == 9U || codepoint == 13U || codepoint == 27U)
        return 0;

    if (bgi::gState.userCharHook != nullptr)
        bgi::gState.userCharHook(codepoint);

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
        bgi::gState.keyQueue.push(static_cast<int>(codepoint));

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_cursor(double xpos, double ypos)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked()) return -1;

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_CURSOR_TRACK)
    {
        bgi::gState.mouseX     = static_cast<int>(xpos);
        bgi::gState.mouseY     = static_cast<int>(ypos);
        bgi::gState.mouseMoved = true;
    }

    if (bgi::gState.userCursorHook != nullptr)
        bgi::gState.userCursorHook(xpos, ypos);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_mouse_button(int button, int action, int mods)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked()) return -1;

    // Note: intentionally omits overlayPerformPick for headless testing

    if (bgi::gState.userMouseButtonHook != nullptr)
        bgi::gState.userMouseButtonHook(button, action, mods);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API int BGI_CALL wxbgi_test_simulate_scroll(double xoffset, double yoffset)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked()) return -1;

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_SCROLL_ACCUM)
    {
        bgi::gState.scrollDeltaX += xoffset;
        bgi::gState.scrollDeltaY += yoffset;
    }

    if (bgi::gState.userScrollHook != nullptr)
        bgi::gState.userScrollHook(xoffset, yoffset);

    bgi::gState.lastResult = bgi::grOk;
    return 0;
}
#endif

BGI_API int BGI_CALL wxbgi_should_close(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
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

    glfwMakeContextCurrent(bgi::gState.window);
    glfwSwapInterval(enabled != 0 ? 1 : 0);
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

    glfwGetWindowSize(bgi::gState.window, width, height);
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

    glfwGetFramebufferSize(bgi::gState.window, width, height);
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
    glfwSetWindowTitle(bgi::gState.window, bgi::gState.windowTitle.c_str());
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

BGI_API double BGI_CALL wxbgi_get_time_seconds(void)
{
    return glfwGetTime();
}

BGI_API void *BGI_CALL wxbgi_get_proc_address(const char *procName)
{
    if (procName == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<void *>(glfwGetProcAddress(procName));
}

BGI_API const char *BGI_CALL wxbgi_get_gl_string(int which)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
    {
        return "";
    }

    glfwMakeContextCurrent(bgi::gState.window);

    GLenum name = GL_VENDOR;
    switch (which)
    {
    case 0:
        name = GL_VENDOR;
        break;
    case 1:
        name = GL_RENDERER;
        break;
    case 2:
        name = GL_VERSION;
        break;
    case 3:
        name = GL_SHADING_LANGUAGE_VERSION;
        break;
    default:
        bgi::gState.lastResult = bgi::grInvalidInput;
        return "";
    }

    const GLubyte *value = glGetString(name);
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

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(bgi::gState.window, &framebufferWidth, &framebufferHeight);

    const long long x2 = static_cast<long long>(x) + static_cast<long long>(width);
    const long long y2 = static_cast<long long>(y) + static_cast<long long>(height);
    if (x < 0 || y < 0 || x2 > framebufferWidth || y2 > framebufferHeight)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

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

// =============================================================================
// User input hooks
// =============================================================================

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
    bgi::gState.userCursorHook = cb;
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

// =============================================================================
// Scroll / Wheel API
// =============================================================================

BGI_API void BGI_CALL wxbgi_get_scroll_delta(double *dx, double *dy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked()) return;
    if (dx) { *dx = bgi::gState.scrollDeltaX; bgi::gState.scrollDeltaX = 0.0; }
    if (dy) { *dy = bgi::gState.scrollDeltaY; bgi::gState.scrollDeltaY = 0.0; }
}

// =============================================================================
// Input default behavior flags
// =============================================================================

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

// =============================================================================
// Hook-context DDS functions (no mutex - only safe inside hook callbacks)
// =============================================================================

BGI_API int BGI_CALL wxbgi_hk_get_mouse_x(void) { return bgi::gState.mouseX; }
BGI_API int BGI_CALL wxbgi_hk_get_mouse_y(void) { return bgi::gState.mouseY; }

BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_count(void)
{
    return static_cast<int>(bgi::gState.selectedObjectIds.size());
}

BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_id(int index, char *buf, int maxLen)
{
    if (index < 0 || static_cast<std::size_t>(index) >= bgi::gState.selectedObjectIds.size())
        return -1;
    const std::string &id = bgi::gState.selectedObjectIds[static_cast<std::size_t>(index)];
    if (buf && maxLen > 0)
    {
        int toCopy = std::min(static_cast<int>(id.size()), maxLen - 1);
        id.copy(buf, static_cast<std::size_t>(toCopy));
        buf[toCopy] = '\0';
    }
    return static_cast<int>(id.size());
}

BGI_API int BGI_CALL wxbgi_hk_dds_is_selected(const char *id)
{
    if (!id) return 0;
    for (const auto &s : bgi::gState.selectedObjectIds)
        if (s == id) return 1;
    return 0;
}

BGI_API void BGI_CALL wxbgi_hk_dds_select(const char *id)
{
    if (!id) return;
    for (const auto &s : bgi::gState.selectedObjectIds)
        if (s == id) return;
    bgi::gState.selectedObjectIds.push_back(id);
}

BGI_API void BGI_CALL wxbgi_hk_dds_deselect(const char *id)
{
    if (!id) return;
    auto &v = bgi::gState.selectedObjectIds;
    v.erase(std::remove(v.begin(), v.end(), std::string(id)), v.end());
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
// ---------------------------------------------------------------------------
// wxWidgets embedding helpers
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_wx_init_for_canvas(int width, int height)
{
    bgi::initForWxCanvas(width, height);
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
            (action == 1) ? 1U : 0U;

    if (bgi::gState.userKeyHook)
        bgi::gState.userKeyHook(glfwKey, glfwKey, action, 0);

    if (action != 1) return;

    if (bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_KEY_QUEUE)
    {
        auto push    = [](int c) { bgi::gState.keyQueue.push(c); };
        auto pushExt = [&push](int sc) { push(0); push(sc); };
        switch (glfwKey)
        {
        case 256: push(27);  break;   // ESCAPE
        case 257:                     // ENTER
        case 335: push(13);  break;   // KP_ENTER
        case 258: push(9);   break;   // TAB
        case 259: push(8);   break;   // BACKSPACE
        case 265: pushExt(72); break; // UP
        case 264: pushExt(80); break; // DOWN
        case 263: pushExt(75); break; // LEFT
        case 262: pushExt(77); break; // RIGHT
        case 268: pushExt(71); break; // HOME
        case 269: pushExt(79); break; // END
        case 266: pushExt(73); break; // PAGE_UP
        case 267: pushExt(81); break; // PAGE_DOWN
        case 260: pushExt(82); break; // INSERT
        case 261: pushExt(83); break; // DELETE
        default:
            if (glfwKey >= 290 && glfwKey <= 301)  // F1-F12
            {
                static const int sc[] = {59,60,61,62,63,64,65,66,67,68,133,134};
                pushExt(sc[glfwKey - 290]);
            }
            break;
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
    if (bgi::gState.userCursorHook)
        bgi::gState.userCursorHook(static_cast<double>(x),
                                   static_cast<double>(y));
}

BGI_API void BGI_CALL wxbgi_wx_mouse_button(int btn, int action)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if ((bgi::gState.inputDefaultFlags & WXBGI_DEFAULT_MOUSE_PICK) &&
        btn == WXBGI_MOUSE_LEFT && action == WXBGI_KEY_PRESS)
    {
        // Pick uses last known mouse position; no ctrl modifier info here.
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
