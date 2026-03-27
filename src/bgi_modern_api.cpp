// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "wx_bgi_ext.h"

#include "bgi_state.h"

#include <algorithm>
#include <cstddef>
#include <mutex>

namespace
{
    bool ensureReadyUnlocked()
    {
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

    glfwPollEvents();
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}

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
