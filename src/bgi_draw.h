#pragma once

#include "bgi_types.h"

#include <array>
#include <utility>
#include <vector>

namespace bgi
{

    int normalizeColor(int color);
    std::uint8_t normalizeColorByte(int value);
    ColorRGB colorToRGB(int c);  ///< resolves any colour index (0-255) to an RGB triple
    std::array<std::uint8_t, kPatternRows> makeFillPatternMask(int pattern);

    bool isReady();
    bool useFillAt(int x, int y);
    bool toDeviceCoordinates(int x, int y, int &screenX, int &screenY);

    void setPixel(int x, int y, int color);
    void setPixelWithMode(int x, int y, int color, int mode);
    int getPixel(int x, int y);
    void clearActivePage(int color);
    void clearViewportRegion(int color);

    void flushToScreen();
    void renderPageToCurrentGLContext(int w, int h);
    void destroyWindowIfNeeded(bool resetGlState = false);

    void drawLineInternal(int x1, int y1, int x2, int y2, int color);
    void drawCircleInternal(int cx, int cy, int radius, int color);
    void drawEllipseInternal(int cx, int cy, int startAngle, int endAngle, int xradius, int yradius, int color);
    void fillRectInternal(int left, int top, int right, int bottom, int color);
    void fillEllipseInternal(int cx, int cy, int xradius, int yradius, int color);
    void drawPolygonInternal(const std::vector<std::pair<int, int>> &points, int color);
    void fillPolygonInternal(const std::vector<std::pair<int, int>> &points, int color);
    std::vector<std::pair<int, int>> buildArcPoints(int cx, int cy, int startAngle, int endAngle, int xradius, int yradius);

} // namespace bgi
