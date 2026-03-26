#pragma once

namespace bgi
{

    // ---------------------------------------------------------------------------
    // Colour helpers
    // ---------------------------------------------------------------------------

    // Wraps an arbitrary colour index into [0, 15].
    int normalizeColor(int color);

    // ---------------------------------------------------------------------------
    // State / window helpers
    // ---------------------------------------------------------------------------

    // Returns true when the window exists and has not been closed.
    // Sets gState.lastResult on failure.
    bool isReady();

    // Whether the fill pattern (set by setfillstyle) covers pixel (x, y).
    bool useFillAt(int x, int y);

    // ---------------------------------------------------------------------------
    // Pixel-level buffer access
    // ---------------------------------------------------------------------------
    void setPixel(int x, int y, int color);
    int getPixel(int x, int y);

    // ---------------------------------------------------------------------------
    // Screen / render helpers
    // ---------------------------------------------------------------------------

    // Redraws the colour-index buffer to the OpenGL window and swaps buffers.
    void flushToScreen();

    // Destroys the GLFW window if currently open.
    void destroyWindowIfNeeded();

    // ---------------------------------------------------------------------------
    // Primitive rasterisers
    // ---------------------------------------------------------------------------
    void drawLineInternal(int x1, int y1, int x2, int y2, int color);
    void drawCircleInternal(int cx, int cy, int radius, int color);

} // namespace bgi
