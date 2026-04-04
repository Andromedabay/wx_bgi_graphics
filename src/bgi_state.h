#pragma once

#include "bgi_types.h"

#include <mutex>

namespace bgi
{

    extern std::mutex gMutex;
    extern BgiState gState;

    void resetPaletteState();
    void resetDrawingState();
    void resetStateForWindow(int width, int height, bool doubleBuffered);
    std::vector<std::uint8_t> &activePageBuffer();
    const std::vector<std::uint8_t> &visualPageBuffer();
    void syncPagesIfNeeded();
    void initForWxCanvas(int width, int height);

} // namespace bgi
