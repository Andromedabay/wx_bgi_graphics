#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace bgi
{

    int currentTextScaleX();
    int currentTextScaleY();
    std::pair<int, int> measureText(const std::string &text);
    void drawGlyph(int x, int y, unsigned char c, int color);
    void drawText(int x, int y, const std::string &text, int color);

} // namespace bgi
