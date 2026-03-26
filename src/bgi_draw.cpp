// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "bgi_draw.h"
#include "bgi_state.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace bgi
{

    // ---------------------------------------------------------------------------
    // Colour helpers
    // ---------------------------------------------------------------------------

    int normalizeColor(int color)
    {
        int wrapped = color % 16;
        if (wrapped < 0)
        {
            wrapped += 16;
        }
        return wrapped;
    }

    // ---------------------------------------------------------------------------
    // State / window helpers
    // ---------------------------------------------------------------------------

    bool isReady()
    {
        if (gState.window == nullptr)
        {
            gState.lastResult = grNoInitGraph;
            return false;
        }
        if (glfwWindowShouldClose(gState.window))
        {
            gState.lastResult = grWindowClosed;
            return false;
        }
        return true;
    }

    bool useFillAt(int x, int y)
    {
        switch (gState.fillPattern)
        {
        case 2:
            return ((x + y) & 1) == 0;
        case 3:
            return (x & 1) == 0;
        case 4:
            return (y & 1) == 0;
        default:
            return true;
        }
    }

    // ---------------------------------------------------------------------------
    // Pixel-level buffer access
    // ---------------------------------------------------------------------------

    void setPixel(int x, int y, int color)
    {
        if (x < 0 || y < 0 || x >= gState.width || y >= gState.height)
        {
            return;
        }
        gState.colorIndexBuffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(gState.width) +
                                static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(normalizeColor(color));
    }

    int getPixel(int x, int y)
    {
        if (x < 0 || y < 0 || x >= gState.width || y >= gState.height)
        {
            return -1;
        }
        return gState.colorIndexBuffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(gState.width) +
                                       static_cast<std::size_t>(x)];
    }

    // ---------------------------------------------------------------------------
    // Screen / render helpers
    // ---------------------------------------------------------------------------

    void flushToScreen()
    {
        if (!isReady())
        {
            return;
        }

        glfwMakeContextCurrent(gState.window);
        glfwPollEvents();

        glViewport(0, 0, gState.width, gState.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0,
                static_cast<double>(gState.width),
                static_cast<double>(gState.height),
                0.0, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_TEXTURE_2D);
        glPointSize(1.0f);
        glBegin(GL_POINTS);
        for (int y = 0; y < gState.height; ++y)
        {
            for (int x = 0; x < gState.width; ++x)
            {
                const int colorIndex = getPixel(x, y);
                if (colorIndex <= 0)
                {
                    continue;
                }

                const ColorRGB color = kBgiPalette[colorIndex];
                glColor3ub(color.r, color.g, color.b);
                glVertex2i(x, y);
            }
        }
        glEnd();
        glFlush();

        glfwSwapBuffers(gState.window);
        gState.lastResult = grOk;
    }

    void destroyWindowIfNeeded()
    {
        if (gState.window != nullptr)
        {
            glfwDestroyWindow(gState.window);
            gState.window = nullptr;
        }
    }

    // ---------------------------------------------------------------------------
    // Primitive rasterisers
    // ---------------------------------------------------------------------------

    void drawLineInternal(int x1, int y1, int x2, int y2, int color)
    {
        // Bresenham's line algorithm
        int dx = std::abs(x2 - x1);
        int sx = x1 < x2 ? 1 : -1;
        int dy = -std::abs(y2 - y1);
        int sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
            setPixel(x1, y1, color);
            if (x1 == x2 && y1 == y2)
            {
                break;
            }
            const int e2 = err * 2;
            if (e2 >= dy)
            {
                err += dy;
                x1 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y1 += sy;
            }
        }
    }

    void drawCircleInternal(int cx, int cy, int radius, int color)
    {
        // Midpoint circle algorithm
        int x = radius;
        int y = 0;
        int decision = 1 - radius;

        while (x >= y)
        {
            setPixel(cx + x, cy + y, color);
            setPixel(cx + y, cy + x, color);
            setPixel(cx - y, cy + x, color);
            setPixel(cx - x, cy + y, color);
            setPixel(cx - x, cy - y, color);
            setPixel(cx - y, cy - x, color);
            setPixel(cx + y, cy - x, color);
            setPixel(cx + x, cy - y, color);

            ++y;
            if (decision < 0)
            {
                decision += 2 * y + 1;
            }
            else
            {
                --x;
                decision += 2 * (y - x) + 1;
            }
        }
    }

} // namespace bgi
