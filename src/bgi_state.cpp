#include "bgi_state.h"

#include <algorithm>

#include "bgi_draw.h"

namespace bgi
{

    const ColorRGB kBgiPalette[16] = {
        {0, 0, 0},       // 0  BLACK
        {0, 0, 170},     // 1  BLUE
        {0, 170, 0},     // 2  GREEN
        {0, 170, 170},   // 3  CYAN
        {170, 0, 0},     // 4  RED
        {170, 0, 170},   // 5  MAGENTA
        {170, 85, 0},    // 6  BROWN
        {170, 170, 170}, // 7  LIGHTGRAY
        {85, 85, 85},    // 8  DARKGRAY
        {85, 85, 255},   // 9  LIGHTBLUE
        {85, 255, 85},   // 10 LIGHTGREEN
        {85, 255, 255},  // 11 LIGHTCYAN
        {255, 85, 85},   // 12 LIGHTRED
        {255, 85, 255},  // 13 LIGHTMAGENTA
        {255, 255, 85},  // 14 YELLOW
        {255, 255, 255}, // 15 WHITE
    };

    std::mutex gMutex;
    BgiState gState;

    void resetPaletteState()
    {
        gState.defaultPalette.size = static_cast<std::uint8_t>(kPaletteSize);
        gState.activePalette.size = static_cast<std::uint8_t>(kPaletteSize);
        for (int index = 0; index < kPaletteSize; ++index)
        {
            gState.defaultPalette.colors[static_cast<std::size_t>(index)] = index;
            gState.activePalette.colors[static_cast<std::size_t>(index)] = index;
            gState.palette[static_cast<std::size_t>(index)] = kBgiPalette[index];
        }
    }

    void resetDrawingState()
    {
        gState.currentColor = WHITE;
        gState.bkColor = BLACK;
        gState.fillPattern = SOLID_FILL;
        gState.fillColor = WHITE;
        gState.fillMask = makeFillPatternMask(SOLID_FILL);
        gState.userFillPatternEnabled = false;
        gState.viewport = {0, 0, gState.width - 1, gState.height - 1, 1};
        gState.currentX = 0;
        gState.currentY = 0;
        gState.lineSettings = {SOLID_LINE, 0xFFFFU, NORM_WIDTH};
        gState.textSettings = {DEFAULT_FONT, HORIZ_DIR, 1, LEFT_TEXT, TOP_TEXT};
        gState.userCharSizeXNum = 1;
        gState.userCharSizeXDen = 1;
        gState.userCharSizeYNum = 1;
        gState.userCharSizeYDen = 1;
        gState.aspectX = 1;
        gState.aspectY = 1;
        gState.writeMode = COPY_PUT;
        gState.lastArc = {0, 0, 0, 0, 0, 0};
        gState.graphBufSize = 0U;
        gState.activePage = 0;
        gState.visualPage = 0;
        while (!gState.keyQueue.empty())
        {
            gState.keyQueue.pop();
        }
        gState.keyDown.fill(0);
        gState.mouseX = 0;
        gState.mouseY = 0;
        resetPaletteState();
    }

    void resetStateForWindow(int width, int height, bool doubleBuffered)
    {
        gState.width = std::max(1, width);
        gState.height = std::max(1, height);
        gState.doubleBuffered = doubleBuffered;
        resetDrawingState();

        const std::size_t pixelCount = static_cast<std::size_t>(gState.width) * static_cast<std::size_t>(gState.height);
        gState.pageBuffers.assign(static_cast<std::size_t>(kPageCount), std::vector<std::uint8_t>(pixelCount, static_cast<std::uint8_t>(gState.bkColor)));
    }

    std::vector<std::uint8_t> &activePageBuffer()
    {
        return gState.pageBuffers[static_cast<std::size_t>(std::clamp(gState.activePage, 0, kPageCount - 1))];
    }

    const std::vector<std::uint8_t> &visualPageBuffer()
    {
        return gState.pageBuffers[static_cast<std::size_t>(std::clamp(gState.visualPage, 0, kPageCount - 1))];
    }

    void syncPagesIfNeeded()
    {
        if (!gState.doubleBuffered || gState.pageBuffers.empty())
        {
            return;
        }
        if (gState.activePage == gState.visualPage)
        {
            return;
        }

        gState.pageBuffers[static_cast<std::size_t>(gState.visualPage)] = gState.pageBuffers[static_cast<std::size_t>(gState.activePage)];
    }

} // namespace bgi
