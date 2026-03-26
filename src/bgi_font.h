#pragma once

#include <array>
#include <cstdint>

namespace bgi
{

    // Returns the 7-row, 5-column bitmap glyph for character c.
    // Letters are case-insensitive; unrecognised characters render as a box.
    const std::array<std::uint8_t, 7> &glyphRows(unsigned char c);

    // Renders a single character glyph into the colour-index buffer at (x, y)
    // using the current drawing colour.
    void drawGlyph(int x, int y, unsigned char c);

} // namespace bgi
