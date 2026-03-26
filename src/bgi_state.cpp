#include "bgi_state.h"

namespace bgi
{

    // ---------------------------------------------------------------------------
    // Standard 16-colour BGI/CGA palette
    // ---------------------------------------------------------------------------
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

} // namespace bgi
