#include "bgi_state.h"

#include <algorithm>

#include "bgi_dds.h"
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

    // BgiState constructor / destructor are defined here (not inline in the
    // header) so the compiler can generate DdsScene constructors/destructors
    // in a translation unit where DdsScene is fully defined (bgi_dds.h above).
    BgiState::BgiState()
    {
        ddsRegistry["default"] = std::make_unique<DdsScene>();
        dds = ddsRegistry["default"].get();
    }
    BgiState::~BgiState() = default;

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
        // Reset extended palette: clear all user-assigned slots back to BLACK.
        gState.extPalette.fill({0, 0, 0});
        gState.extColorNext = kExtColorBase;
        // Reserve internal slots for selection-flash colours.
        gState.extPalette[252 - kExtColorBase] = {230, 120, 20};   // index 252: orange
        gState.extPalette[253 - kExtColorBase] = {150, 30, 210};   // index 253: purple
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
        gState.mouseX     = 0;
        gState.mouseY     = 0;
        gState.mouseMoved = false;
        gState.scrollDeltaX = 0.0;
        gState.scrollDeltaY = 0.0;
        gState.inputDefaultFlags = WXBGI_DEFAULT_ALL;
        gState.selectedObjectIds.clear();
        gState.selectionFlashScheme  = 0;
        gState.selectionPickRadiusPx = 16;
        gState.solidColorOverride    = -1;
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

        // Rebuild the DDS scene registry: destroy all non-default scenes, reset
        // the default scene to empty.  Camera / UCS registries are rebuilt below.
        for (auto it = gState.ddsRegistry.begin(); it != gState.ddsRegistry.end(); )
        {
            if (it->first != "default")
                it = gState.ddsRegistry.erase(it);
            else
                ++it;
        }
        if (gState.ddsRegistry.find("default") == gState.ddsRegistry.end())
            gState.ddsRegistry["default"] = std::make_unique<DdsScene>();
        else
            gState.ddsRegistry["default"]->clearAll();
        gState.activeDdsName = "default";
        gState.dds = gState.ddsRegistry["default"].get();

        gState.cameras.clear();
        gState.ucsSystems.clear();
        gState.pendingGl.clear();
        gState.pendingGlQueue.clear();

        // Create the default pixel-space camera that replicates classic BGI
        // coordinates: (0,0) = top-left, Y increases downward.
        // The Y-flip is achieved by setting orthoBottom > orthoTop.
        {
            auto dc = std::make_shared<DdsCamera>();
            dc->name                   = "default";
            dc->camera.projection      = CameraProjection::Orthographic;
            dc->camera.eyeX            = 0.f;
            dc->camera.eyeY            = 0.f;
            dc->camera.eyeZ            = 1.f;
            dc->camera.targetX         = 0.f;
            dc->camera.targetY         = 0.f;
            dc->camera.targetZ         = 0.f;
            dc->camera.upX             = 0.f;
            dc->camera.upY             = 1.f;
            dc->camera.upZ             = 0.f;
            dc->camera.orthoLeft       = 0.f;
            dc->camera.orthoRight      = static_cast<float>(gState.width);
            dc->camera.orthoBottom     = static_cast<float>(gState.height); // Y-flip
            dc->camera.orthoTop        = 0.f;
            dc->camera.nearPlane       = -1.f;
            dc->camera.farPlane        =  1.f;
            dc->camera.assignedSceneName = "default";
            gState.dds->append(dc);
            gState.cameras["default"]  = dc;
        }
        gState.activeCamera = "default";

        // Create the identity "world" UCS (cannot be destroyed).
        {
            auto du = std::make_shared<DdsUcs>();
            du->name           = "world";
            du->ucs            = CoordSystem{};
            gState.dds->append(du);
            gState.ucsSystems["world"] = du;
        }
        gState.activeUcs = "world";

        // Reset world extents to "empty".
        gState.worldExtents = WorldExtents{};
    }

    void resizePixelBuffer(int width, int height)
    {
        // Resize the pixel buffer to the new dimensions while preserving all
        // DDS scenes, cameras, UCS systems, and drawing state.  Called by
        // wxbgi_wx_resize() when an embedded canvas is resized by the OS —
        // a resize must NOT wipe user-created scene graphs.
        gState.width  = std::max(1, width);
        gState.height = std::max(1, height);

        const std::size_t pixelCount =
            static_cast<std::size_t>(gState.width) *
            static_cast<std::size_t>(gState.height);
        gState.pageBuffers.assign(
            static_cast<std::size_t>(kPageCount),
            std::vector<std::uint8_t>(pixelCount, static_cast<std::uint8_t>(gState.bkColor)));

        // Update viewport to full new window size.
        gState.viewport = {0, 0, gState.width - 1, gState.height - 1, false};
        gState.currentX = 0;
        gState.currentY = 0;
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

    void initForWxCanvas(int width, int height)
    {
        gState.wxEmbedded = true;
        resetStateForWindow(std::max(1, width), std::max(1, height), true);
        gState.lastResult = grOk;
    }

} // namespace bgi
