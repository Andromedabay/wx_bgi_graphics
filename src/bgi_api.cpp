// Public BGI-compatible API exported from the shared library.
// All functions lock gMutex before accessing gState.

// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "bgi_types.h"
#include "bgi_state.h"
#include "bgi_draw.h"
#include "bgi_font.h"

#include <cstddef>
#include <mutex>
#include <queue>
#include <string>

// ---------------------------------------------------------------------------
// initgraph — create window and initialise OpenGL context
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL initgraph(int *graphdriver, int *graphmode, char *pathtodriver)
{
    (void)pathtodriver;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (graphdriver != nullptr)
    {
        *graphdriver = 0;
    }
    if (graphmode != nullptr)
    {
        *graphmode = 0;
    }

    bgi::destroyWindowIfNeeded();

    if (!bgi::gState.glfwInitialized)
    {
        if (glfwInit() == GLFW_FALSE)
        {
            bgi::gState.lastResult = bgi::grInitFailed;
            return;
        }
        bgi::gState.glfwInitialized = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    bgi::gState.window = glfwCreateWindow(
        bgi::gState.width, bgi::gState.height,
        "BGI OpenGL Wrapper", nullptr, nullptr);

    if (bgi::gState.window == nullptr)
    {
        bgi::gState.lastResult = bgi::grInitFailed;
        return;
    }

    glfwMakeContextCurrent(bgi::gState.window);
    glfwSwapInterval(1);

#if !defined(__APPLE__)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        bgi::destroyWindowIfNeeded();
        bgi::gState.lastResult = bgi::grInitFailed;
        return;
    }
#endif

    const std::size_t pixelCount =
        static_cast<std::size_t>(bgi::gState.width) *
        static_cast<std::size_t>(bgi::gState.height);

    bgi::gState.colorIndexBuffer.assign(pixelCount, 0U);
    bgi::gState.rgbaBuffer.assign(pixelCount * 4U, 0U);
    bgi::gState.currentColor = 15;
    bgi::gState.fillColor = 15;
    bgi::gState.fillPattern = 1;
    bgi::gState.lastResult = bgi::grOk;

    bgi::flushToScreen();
}

// ---------------------------------------------------------------------------
// closegraph — destroy window and shut down GLFW
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL closegraph(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    bgi::destroyWindowIfNeeded();

    if (bgi::gState.glfwInitialized)
    {
        glfwTerminate();
        bgi::gState.glfwInitialized = false;
    }

    bgi::gState.colorIndexBuffer.clear();
    bgi::gState.rgbaBuffer.clear();
    bgi::gState.lastResult = bgi::grNoInitGraph;
}

// ---------------------------------------------------------------------------
// graphresult — return the last operation's error code
// ---------------------------------------------------------------------------
BGI_API int BGI_CALL graphresult(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.lastResult;
}

// ---------------------------------------------------------------------------
// setcolor — set the active drawing colour (0-15)
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL setcolor(int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.currentColor = bgi::normalizeColor(color);
    bgi::gState.lastResult = bgi::grOk;
}

// ---------------------------------------------------------------------------
// setfillstyle — choose fill pattern and fill colour
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL setfillstyle(int pattern, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.fillPattern = pattern;
    bgi::gState.fillColor = bgi::normalizeColor(color);
    bgi::gState.lastResult = bgi::grOk;
}

// ---------------------------------------------------------------------------
// line — draw a line between two points
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL line(int x1, int y1, int x2, int y2)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady())
    {
        return;
    }

    bgi::drawLineInternal(x1, y1, x2, y2, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// ---------------------------------------------------------------------------
// circle — draw a circle outline
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL circle(int x, int y, int radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady() || radius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::drawCircleInternal(x, y, radius, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// ---------------------------------------------------------------------------
// rectangle — draw a hollow rectangle
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL rectangle(int left, int top, int right, int bottom)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady())
    {
        return;
    }

    bgi::drawLineInternal(left, top, right, top, bgi::gState.currentColor);
    bgi::drawLineInternal(right, top, right, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(right, bottom, left, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(left, bottom, left, top, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// ---------------------------------------------------------------------------
// outtextxy — render a text string at (x, y)
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL outtextxy(int x, int y, char *textstring)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady())
    {
        return;
    }

    if (textstring == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    const std::string text(textstring);
    int penX = x;
    for (char ch : text)
    {
        bgi::drawGlyph(penX, y, static_cast<unsigned char>(ch));
        penX += 6;
    }
    bgi::flushToScreen();
}

// ---------------------------------------------------------------------------
// floodfill — boundary flood fill using the active fill style
// ---------------------------------------------------------------------------
BGI_API void BGI_CALL floodfill(int x, int y, int border)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady())
    {
        return;
    }

    if (x < 0 || y < 0 || x >= bgi::gState.width || y >= bgi::gState.height)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    const int borderColor = bgi::normalizeColor(border);
    const int targetColor = bgi::getPixel(x, y);
    if (targetColor < 0 || targetColor == borderColor)
    {
        bgi::gState.lastResult = bgi::grOk;
        return;
    }

    std::queue<std::pair<int, int>> q;
    q.push({x, y});

    while (!q.empty())
    {
        const auto [cx, cy] = q.front();
        q.pop();

        if (cx < 0 || cy < 0 || cx >= bgi::gState.width || cy >= bgi::gState.height)
        {
            continue;
        }

        const int current = bgi::getPixel(cx, cy);
        if (current != targetColor || current == borderColor)
        {
            continue;
        }

        if (bgi::useFillAt(cx, cy))
        {
            bgi::setPixel(cx, cy, bgi::gState.fillColor);
        }
        else
        {
            bgi::setPixel(cx, cy, bgi::gState.currentColor);
        }

        q.push({cx + 1, cy});
        q.push({cx - 1, cy});
        q.push({cx, cy + 1});
        q.push({cx, cy - 1});
    }

    bgi::flushToScreen();
}
