#include "wx_bgi_ext.h"

#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_state.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

namespace
{
    constexpr int kFieldPaletteBase  = 192;
    constexpr int kFieldPaletteCount = 48;

    struct PaletteStop
    {
        float pos;
        bgi::ColorRGB rgb;
    };

    using PaletteStops = std::array<PaletteStop, 5>;

    constexpr PaletteStops kGrayStops{{
        {0.00f, {  0,   0,   0}},
        {0.25f, { 64,  64,  64}},
        {0.50f, {128, 128, 128}},
        {0.75f, {192, 192, 192}},
        {1.00f, {255, 255, 255}},
    }};

    constexpr PaletteStops kViridisStops{{
        {0.00f, { 68,   1,  84}},
        {0.25f, { 59,  82, 139}},
        {0.50f, { 33, 145, 140}},
        {0.75f, { 94, 201,  98}},
        {1.00f, {253, 231,  37}},
    }};

    constexpr PaletteStops kInfernoStops{{
        {0.00f, {  0,   0,   4}},
        {0.25f, { 87,  15, 109}},
        {0.50f, {187,  55,  84}},
        {0.75f, {249, 142,   8}},
        {1.00f, {252, 255, 164}},
    }};

    constexpr PaletteStops kTurboStops{{
        {0.00f, { 48,  18,  59}},
        {0.25f, { 38, 188, 225}},
        {0.50f, { 82, 246, 103}},
        {0.75f, {252, 185,  56}},
        {1.00f, {122,   4,   2}},
    }};

    bool ensureReadyUnlocked()
    {
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
        bgi::gState.lastResult = bgi::grOk;
        return true;
    }

    const PaletteStops& paletteStopsFor(int paletteMode)
    {
        switch (paletteMode)
        {
        case WXBGI_FIELD_PALETTE_GRAYSCALE: return kGrayStops;
        case WXBGI_FIELD_PALETTE_VIRIDIS:   return kViridisStops;
        case WXBGI_FIELD_PALETTE_INFERNO:   return kInfernoStops;
        case WXBGI_FIELD_PALETTE_TURBO:
        default:                            return kTurboStops;
        }
    }

    bgi::ColorRGB lerp(const bgi::ColorRGB& a, const bgi::ColorRGB& b, float t)
    {
        const auto mix = [t](std::uint8_t av, std::uint8_t bv) -> std::uint8_t {
            const float v = static_cast<float>(av) +
                            (static_cast<float>(bv) - static_cast<float>(av)) * t;
            return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0l, 255l));
        };
        return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
    }

    bgi::ColorRGB samplePaletteColor(int paletteMode, float t)
    {
        const auto& stops = paletteStopsFor(paletteMode);
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        for (std::size_t i = 1; i < stops.size(); ++i)
        {
            if (clamped <= stops[i].pos)
            {
                const float span = std::max(1e-6f, stops[i].pos - stops[i - 1].pos);
                const float local = (clamped - stops[i - 1].pos) / span;
                return lerp(stops[i - 1].rgb, stops[i].rgb, local);
            }
        }
        return stops.back().rgb;
    }

    void bakeFieldPalette(int paletteMode)
    {
        for (int i = 0; i < kFieldPaletteCount; ++i)
        {
            const float t = (kFieldPaletteCount > 1)
                ? static_cast<float>(i) / static_cast<float>(kFieldPaletteCount - 1)
                : 0.0f;
            bgi::gState.extPalette[static_cast<std::size_t>(kFieldPaletteBase - bgi::kExtColorBase + i)] =
                samplePaletteColor(paletteMode, t);
        }
    }

    int paletteIndexFor(float value, float minValue, float maxValue)
    {
        if (!std::isfinite(value))
            return kFieldPaletteBase;
        const float denom = maxValue - minValue;
        const float t = (denom > 1e-12f)
            ? (value - minValue) / denom
            : 0.0f;
        const int slot = static_cast<int>(std::lround(std::clamp(t, 0.0f, 1.0f) *
                                                      static_cast<float>(kFieldPaletteCount - 1)));
        return kFieldPaletteBase + slot;
    }

    void autoRange(const float* values, int count, float& minValue, float& maxValue)
    {
        if (count <= 0)
            return;

        bool foundFinite = false;
        float minSeen = 0.0f;
        float maxSeen = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const float value = values[i];
            if (!std::isfinite(value))
                continue;
            if (!foundFinite)
            {
                minSeen = value;
                maxSeen = value;
                foundFinite = true;
                continue;
            }
            minSeen = std::min(minSeen, value);
            maxSeen = std::max(maxSeen, value);
        }

        if (!foundFinite)
        {
            minValue = 0.0f;
            maxValue = 1.0f;
            return;
        }

        minValue = minSeen;
        maxValue = maxSeen;
        if (!(maxValue > minValue))
            maxValue = minValue + 1.0f;
    }

    void drawArrow(int x0, int y0, int x1, int y1, int color)
    {
        bgi::drawLineInternal(x0, y0, x1, y1, color);

        const float dx = static_cast<float>(x1 - x0);
        const float dy = static_cast<float>(y1 - y0);
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 2.0f)
            return;

        const float ux = dx / len;
        const float uy = dy / len;
        const float headLen = std::min(6.0f, len * 0.35f);
        const float side = headLen * 0.6f;

        const int hx0 = static_cast<int>(std::lround(static_cast<float>(x1) - ux * headLen - uy * side));
        const int hy0 = static_cast<int>(std::lround(static_cast<float>(y1) - uy * headLen + ux * side));
        const int hx1 = static_cast<int>(std::lround(static_cast<float>(x1) - ux * headLen + uy * side));
        const int hy1 = static_cast<int>(std::lround(static_cast<float>(y1) - uy * headLen - ux * side));

        bgi::drawLineInternal(x1, y1, hx0, hy0, color);
        bgi::drawLineInternal(x1, y1, hx1, hy1, color);
    }

    std::string formatValue(float value)
    {
        if (!std::isfinite(value))
            return "n/a";
        char buf[48] = {};
        std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(value));
        return std::string(buf);
    }
}

BGI_API int BGI_CALL wxbgi_field_draw_scalar_grid(int left, int top,
                                                  int cols, int rows,
                                                  const float *values, int valueCount,
                                                  int cellSizePx,
                                                  float minValue, float maxValue,
                                                  int paletteMode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || values == nullptr || cols <= 0 || rows <= 0 ||
        valueCount < cols * rows || cellSizePx <= 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    if (!(maxValue > minValue))
        autoRange(values, cols * rows, minValue, maxValue);

    bakeFieldPalette(paletteMode);

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const int idx = row * cols + col;
            const float value = std::isfinite(values[idx]) ? values[idx] : minValue;
            const int color = paletteIndexFor(value, minValue, maxValue);
            const int x0 = left + col * cellSizePx;
            const int y0 = top  + row * cellSizePx;
            if (cellSizePx == 1)
                bgi::setPixel(x0, y0, color);
            else
                bgi::fillRectInternal(x0, y0, x0 + cellSizePx - 1, y0 + cellSizePx - 1, color);
        }
    }

    bgi::gState.lastResult = bgi::grOk;
    bgi::flushToScreen();
    return 1;
}

BGI_API int BGI_CALL wxbgi_field_draw_vector_grid(int left, int top,
                                                  int cols, int rows,
                                                  const float *vectorsXY, int vectorCount,
                                                  int cellSizePx,
                                                  float scale,
                                                  int stride,
                                                  int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || vectorsXY == nullptr || cols <= 0 || rows <= 0 ||
        vectorCount < cols * rows * 2 || cellSizePx <= 0 || stride <= 0 || scale < 0.0f)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    for (int row = 0; row < rows; row += stride)
    {
        for (int col = 0; col < cols; col += stride)
        {
            const int idx = (row * cols + col) * 2;
            const float vx = vectorsXY[idx + 0];
            const float vy = vectorsXY[idx + 1];
            if (!std::isfinite(vx) || !std::isfinite(vy))
                continue;
            const float mag = std::sqrt(vx * vx + vy * vy);
            if (!std::isfinite(mag) || mag < 1e-6f)
                continue;

            const int cx = left + col * cellSizePx + cellSizePx / 2;
            const int cy = top  + row * cellSizePx + cellSizePx / 2;
            const float maxLen = static_cast<float>(cellSizePx * stride) * 0.48f;
            const float len = std::min(maxLen, mag * scale * static_cast<float>(cellSizePx));
            const float invMag = len / mag;

            const int x1 = static_cast<int>(std::lround(static_cast<float>(cx) + vx * invMag));
            const int y1 = static_cast<int>(std::lround(static_cast<float>(cy) - vy * invMag));
            drawArrow(cx, cy, x1, y1, color);
        }
    }

    bgi::gState.lastResult = bgi::grOk;
    bgi::flushToScreen();
    return 1;
}

BGI_API int BGI_CALL wxbgi_field_draw_scalar_legend(int left, int top,
                                                    int width, int height,
                                                    float minValue, float maxValue,
                                                    int paletteMode,
                                                    const char *label)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked() || width <= 0 || height <= 1)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return -1;
    }

    bakeFieldPalette(paletteMode);

    for (int y = 0; y < height; ++y)
    {
        const float t = 1.0f - static_cast<float>(y) / static_cast<float>(height - 1);
        const int color = paletteIndexFor(t, 0.0f, 1.0f);
        bgi::fillRectInternal(left, top + y, left + width - 1, top + y, color);
    }

    bgi::drawLineInternal(left, top, left + width - 1, top, bgi::WHITE);
    bgi::drawLineInternal(left, top + height - 1, left + width - 1, top + height - 1, bgi::WHITE);
    bgi::drawLineInternal(left, top, left, top + height - 1, bgi::WHITE);
    bgi::drawLineInternal(left + width - 1, top, left + width - 1, top + height - 1, bgi::WHITE);

    if (label != nullptr && label[0] != '\0')
        bgi::drawText(left + width + 6, top, label, bgi::WHITE);

    bgi::drawText(left + width + 6, top + 12, formatValue(maxValue), bgi::LIGHTGRAY);
    bgi::drawText(left + width + 6, top + height - 12, formatValue(minValue), bgi::LIGHTGRAY);

    bgi::gState.lastResult = bgi::grOk;
    bgi::flushToScreen();
    return 1;
}
