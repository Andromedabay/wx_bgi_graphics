// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "bgi_draw.h"
#include "bgi_overlay.h"
#include "bgi_state.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace bgi
{

    namespace
    {
        constexpr double kDegreesPerStep = 1.0;

        int viewportWidth()
        {
            return std::max(0, gState.viewport.right - gState.viewport.left + 1);
        }

        int viewportHeight()
        {
            return std::max(0, gState.viewport.bottom - gState.viewport.top + 1);
        }

        bool isInDeviceBounds(int screenX, int screenY)
        {
            return screenX >= 0 && screenY >= 0 && screenX < gState.width && screenY < gState.height;
        }

        bool isInViewportBounds(int screenX, int screenY)
        {
            return screenX >= gState.viewport.left && screenY >= gState.viewport.top &&
                   screenX <= gState.viewport.right && screenY <= gState.viewport.bottom;
        }

        std::uint8_t applyWriteMode(std::uint8_t destination, int color, int mode)
        {
            const std::uint8_t source = static_cast<std::uint8_t>(normalizeColor(color));
            // Bitwise write modes (XOR/OR/AND/NOT) are 4-bit palette operations.
            // Extended colours (>= kExtColorBase) are always written as COPY_PUT.
            if (source >= static_cast<std::uint8_t>(kExtColorBase))
                return source;
            switch (mode)
            {
            case XOR_PUT:
                return static_cast<std::uint8_t>((destination ^ source) & 0x0F);
            case OR_PUT:
                return static_cast<std::uint8_t>((destination | source) & 0x0F);
            case AND_PUT:
                return static_cast<std::uint8_t>((destination & source) & 0x0F);
            case NOT_PUT:
                return static_cast<std::uint8_t>((~source) & 0x0F);
            case COPY_PUT:
            default:
                return source;
            }
        }

        void setDevicePixelWithMode(int screenX, int screenY, int color, int mode)
        {
            if (!isInDeviceBounds(screenX, screenY))
            {
                return;
            }

            auto &buffer = activePageBuffer();
            const std::size_t index = static_cast<std::size_t>(screenY) * static_cast<std::size_t>(gState.width) +
                                      static_cast<std::size_t>(screenX);
            buffer[index] = applyWriteMode(buffer[index], color, mode);
        }

        void stampPixel(int x, int y, int color)
        {
            const int thickness = std::max(1, gState.lineSettings.thickness);
            const int radius = thickness / 2;
            for (int offsetY = -radius; offsetY <= radius; ++offsetY)
            {
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    setPixelWithMode(x + offsetX, y + offsetY, color, gState.writeMode);
                }
            }
        }

        unsigned linePatternMask()
        {
            switch (gState.lineSettings.linestyle)
            {
            case DOTTED_LINE:
                return 0xCCCCU;
            case CENTER_LINE:
                return 0xF3CFU;
            case DASHED_LINE:
                return 0xF0F0U;
            case USERBIT_LINE:
                return gState.lineSettings.upattern == 0U ? 0xFFFFU : gState.lineSettings.upattern;
            case SOLID_LINE:
            default:
                return 0xFFFFU;
            }
        }

        std::vector<std::pair<int, int>> normalizedPolygon(const std::vector<std::pair<int, int>> &points)
        {
            if (!points.empty() && points.front() == points.back())
            {
                return points;
            }

            std::vector<std::pair<int, int>> closed = points;
            if (!closed.empty())
            {
                closed.push_back(closed.front());
            }
            return closed;
        }

        int normalizedStartAngle(int angle)
        {
            int value = angle % 360;
            if (value < 0)
            {
                value += 360;
            }
            return value;
        }

        double degreesToRadians(double degrees)
        {
            return degrees * std::numbers::pi / 180.0;
        }
    } // namespace

    int normalizeColor(int color)
    {
        // Extended colours (16-255) pass through unchanged so they address extPalette.
        if (color >= kExtColorBase && color < kExtColorBase + kExtPaletteSize)
            return color;
        // Classic BGI palette (0-15): wrap negatives and out-of-range positives.
        int wrapped = color % kPaletteSize;
        if (wrapped < 0)
            wrapped += kPaletteSize;
        return wrapped;
    }

    ColorRGB colorToRGB(int c)
    {
        if (c >= kExtColorBase && c < kExtColorBase + kExtPaletteSize)
            return gState.extPalette[static_cast<std::size_t>(c - kExtColorBase)];
        return gState.palette[static_cast<std::size_t>(normalizeColor(c))];
    }

    std::uint8_t normalizeColorByte(int value)
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
    }

    std::array<std::uint8_t, kPatternRows> makeFillPatternMask(int pattern)
    {
        switch (pattern)
        {
        case EMPTY_FILL:
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        case LINE_FILL:
            return {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
        case LTSLASH_FILL:
            return {0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04};
        case SLASH_FILL:
            return {0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88};
        case BKSLASH_FILL:
            return {0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11};
        case LTBKSLASH_FILL:
            return {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
        case HATCH_FILL:
            return {0xFF, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11};
        case XHATCH_FILL:
            return {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
        case INTERLEAVE_FILL:
            return {0x33, 0x66, 0xCC, 0x99, 0x33, 0x66, 0xCC, 0x99};
        case WIDE_DOT_FILL:
            return {0x88, 0x00, 0x00, 0x22, 0x00, 0x00, 0x88, 0x00};
        case CLOSE_DOT_FILL:
            return {0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00};
        case SOLID_FILL:
        case USER_FILL:
        default:
            return {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        }
    }

    bool isReady()
    {
        if (gState.window == nullptr)
        {
            gState.lastResult = grNoInitGraph;
            return false;
        }
        if (glfwWindowShouldClose(gState.window) != 0)
        {
            gState.lastResult = grWindowClosed;
            return false;
        }
        if (gState.pageBuffers.empty())
        {
            gState.lastResult = grNoInitGraph;
            return false;
        }
        return true;
    }

    bool useFillAt(int x, int y)
    {
        const auto row = gState.fillMask[static_cast<std::size_t>(y & 7)];
        const auto mask = static_cast<std::uint8_t>(1U << (7 - (x & 7)));
        return (row & mask) != 0U;
    }

    bool toDeviceCoordinates(int x, int y, int &screenX, int &screenY)
    {
        screenX = gState.viewport.left + x;
        screenY = gState.viewport.top + y;
        if (!isInDeviceBounds(screenX, screenY))
        {
            return false;
        }
        if (gState.viewport.clip != 0 && !isInViewportBounds(screenX, screenY))
        {
            return false;
        }
        return true;
    }

    void setPixelWithMode(int x, int y, int color, int mode)
    {
        int screenX = 0;
        int screenY = 0;
        if (!toDeviceCoordinates(x, y, screenX, screenY))
        {
            return;
        }
        setDevicePixelWithMode(screenX, screenY, color, mode);
    }

    void setPixel(int x, int y, int color)
    {
        setPixelWithMode(x, y, color, gState.writeMode);
    }

    int getPixel(int x, int y)
    {
        int screenX = 0;
        int screenY = 0;
        if (!toDeviceCoordinates(x, y, screenX, screenY))
        {
            return -1;
        }

        const auto &buffer = activePageBuffer();
        const std::size_t index = static_cast<std::size_t>(screenY) * static_cast<std::size_t>(gState.width) +
                                  static_cast<std::size_t>(screenX);
        return buffer[index];
    }

    void clearActivePage(int color)
    {
        if (gState.pageBuffers.empty())
        {
            return;
        }

        auto &buffer = activePageBuffer();
        std::fill(buffer.begin(), buffer.end(), static_cast<std::uint8_t>(normalizeColor(color)));
    }

    void clearViewportRegion(int color)
    {
        for (int y = 0; y < viewportHeight(); ++y)
        {
            for (int x = 0; x < viewportWidth(); ++x)
            {
                setPixelWithMode(x, y, color, COPY_PUT);
            }
        }
    }

    void flushToScreen()
    {
        if (!isReady())
        {
            return;
        }

        glfwMakeContextCurrent(gState.window);
        glfwPollEvents();

        const ColorRGB background = colorToRGB(gState.bkColor);

        glViewport(0, 0, gState.width, gState.height);
        glClearColor(background.r / 255.0f, background.g / 255.0f, background.b / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(gState.width), static_cast<double>(gState.height), 0.0, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_TEXTURE_2D);
        glPointSize(1.0f);

        const auto &buffer = visualPageBuffer();
        glBegin(GL_POINTS);
        for (int y = 0; y < gState.height; ++y)
        {
            for (int x = 0; x < gState.width; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(gState.width) +
                                          static_cast<std::size_t>(x);
                const int colorIndex = buffer[index];
                if (colorIndex == normalizeColor(gState.bkColor))
                {
                    continue;
                }

                const ColorRGB color = colorToRGB(colorIndex);
                glColor3ub(color.r, color.g, color.b);
                glVertex2i(x, y);
            }
        }
        glEnd();

        // Draw selection cursor squares for cameras that have the cursor enabled.
        // These are GL primitives painted on top of the page-buffer pixels.
        bgi::drawSelectionCursorsGL();

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

    void drawLineInternal(int x1, int y1, int x2, int y2, int color)
    {
        int dx = std::abs(x2 - x1);
        int sx = x1 < x2 ? 1 : -1;
        int dy = -std::abs(y2 - y1);
        int sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        const unsigned pattern = linePatternMask();
        int step = 0;

        while (true)
        {
            if ((pattern & (0x8000U >> (step & 15))) != 0U)
            {
                stampPixel(x1, y1, color);
            }

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
            ++step;
        }
    }

    void drawCircleInternal(int cx, int cy, int radius, int color)
    {
        int x = radius;
        int y = 0;
        int decision = 1 - radius;

        while (x >= y)
        {
            stampPixel(cx + x, cy + y, color);
            stampPixel(cx + y, cy + x, color);
            stampPixel(cx - y, cy + x, color);
            stampPixel(cx - x, cy + y, color);
            stampPixel(cx - x, cy - y, color);
            stampPixel(cx - y, cy - x, color);
            stampPixel(cx + y, cy - x, color);
            stampPixel(cx + x, cy - y, color);

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

    std::vector<std::pair<int, int>> buildArcPoints(int cx, int cy, int startAngle, int endAngle, int xradius, int yradius)
    {
        std::vector<std::pair<int, int>> points;
        const int start = normalizedStartAngle(startAngle);
        int end = normalizedStartAngle(endAngle);
        if (end < start)
        {
            end += 360;
        }

        for (double angle = static_cast<double>(start); angle <= static_cast<double>(end); angle += kDegreesPerStep)
        {
            const double radians = degreesToRadians(angle);
            const int x = cx + static_cast<int>(std::lround(std::cos(radians) * static_cast<double>(xradius)));
            const int y = cy - static_cast<int>(std::lround(std::sin(radians) * static_cast<double>(yradius)));
            if (points.empty() || points.back() != std::pair<int, int>{x, y})
            {
                points.emplace_back(x, y);
            }
        }

        if (points.empty())
        {
            points.emplace_back(cx + xradius, cy);
        }
        return points;
    }

    void drawEllipseInternal(int cx, int cy, int startAngle, int endAngle, int xradius, int yradius, int color)
    {
        const auto points = buildArcPoints(cx, cy, startAngle, endAngle, xradius, yradius);
        for (std::size_t index = 1; index < points.size(); ++index)
        {
            drawLineInternal(points[index - 1].first, points[index - 1].second, points[index].first, points[index].second, color);
        }
    }

    void fillRectInternal(int left, int top, int right, int bottom, int color)
    {
        const int minX = std::min(left, right);
        const int maxX = std::max(left, right);
        const int minY = std::min(top, bottom);
        const int maxY = std::max(top, bottom);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (useFillAt(x, y))
                {
                    setPixelWithMode(x, y, color, COPY_PUT);
                }
            }
        }
    }

    void fillEllipseInternal(int cx, int cy, int xradius, int yradius, int color)
    {
        if (xradius < 0 || yradius < 0)
        {
            return;
        }

        for (int y = -yradius; y <= yradius; ++y)
        {
            const double yNorm = yradius == 0 ? 0.0 : static_cast<double>(y * y) / static_cast<double>(yradius * yradius);
            if (yNorm > 1.0)
            {
                continue;
            }
            const double xSpan = static_cast<double>(xradius) * std::sqrt(std::max(0.0, 1.0 - yNorm));
            const int extent = static_cast<int>(std::floor(xSpan));
            for (int x = -extent; x <= extent; ++x)
            {
                if (useFillAt(cx + x, cy + y))
                {
                    setPixelWithMode(cx + x, cy + y, color, COPY_PUT);
                }
            }
        }
    }

    void drawPolygonInternal(const std::vector<std::pair<int, int>> &points, int color)
    {
        const auto closed = normalizedPolygon(points);
        if (closed.size() < 2)
        {
            return;
        }

        for (std::size_t index = 1; index < closed.size(); ++index)
        {
            drawLineInternal(closed[index - 1].first, closed[index - 1].second, closed[index].first, closed[index].second, color);
        }
    }

    void fillPolygonInternal(const std::vector<std::pair<int, int>> &points, int color)
    {
        if (points.size() < 3)
        {
            return;
        }

        const auto polygon = normalizedPolygon(points);
        int minY = polygon.front().second;
        int maxY = polygon.front().second;
        for (const auto &[x, y] : polygon)
        {
            (void)x;
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            std::vector<int> intersections;
            for (std::size_t index = 1; index < polygon.size(); ++index)
            {
                const auto &[x1, y1] = polygon[index - 1];
                const auto &[x2, y2] = polygon[index];
                const bool crosses = ((y1 <= y) && (y2 > y)) || ((y2 <= y) && (y1 > y));
                if (!crosses || y1 == y2)
                {
                    continue;
                }

                const double x = static_cast<double>(x1) +
                                 (static_cast<double>(y - y1) * static_cast<double>(x2 - x1) / static_cast<double>(y2 - y1));
                intersections.push_back(static_cast<int>(std::lround(x)));
            }

            std::sort(intersections.begin(), intersections.end());
            for (std::size_t index = 1; index < intersections.size(); index += 2)
            {
                for (int x = intersections[index - 1]; x <= intersections[index]; ++x)
                {
                    if (useFillAt(x, y))
                    {
                        setPixelWithMode(x, y, color, COPY_PUT);
                    }
                }
            }
        }
    }

} // namespace bgi
