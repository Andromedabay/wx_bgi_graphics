#pragma once

#include <array>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

#if defined(_WIN32)
#define BGI_API extern "C" __declspec(dllexport)
#else
#define BGI_API extern "C"
#endif

#if defined(_MSC_VER)
#define BGI_CALL __cdecl
#else
#define BGI_CALL
#endif

struct GLFWwindow;

namespace bgi
{

    constexpr int DETECT = 0;
    constexpr int kDefaultWidth = 960;
    constexpr int kDefaultHeight = 720;
    constexpr int kPaletteSize = 16;
    constexpr int kPageCount = 2;
    constexpr int kPatternRows = 8;
    constexpr int kPatternCols = 8;

    constexpr int BLACK = 0;
    constexpr int BLUE = 1;
    constexpr int GREEN = 2;
    constexpr int CYAN = 3;
    constexpr int RED = 4;
    constexpr int MAGENTA = 5;
    constexpr int BROWN = 6;
    constexpr int LIGHTGRAY = 7;
    constexpr int DARKGRAY = 8;
    constexpr int LIGHTBLUE = 9;
    constexpr int LIGHTGREEN = 10;
    constexpr int LIGHTCYAN = 11;
    constexpr int LIGHTRED = 12;
    constexpr int LIGHTMAGENTA = 13;
    constexpr int YELLOW = 14;
    constexpr int WHITE = 15;

    constexpr int SOLID_LINE = 0;
    constexpr int DOTTED_LINE = 1;
    constexpr int CENTER_LINE = 2;
    constexpr int DASHED_LINE = 3;
    constexpr int USERBIT_LINE = 4;

    constexpr int NORM_WIDTH = 1;
    constexpr int THICK_WIDTH = 3;

    constexpr int EMPTY_FILL = 0;
    constexpr int SOLID_FILL = 1;
    constexpr int LINE_FILL = 2;
    constexpr int LTSLASH_FILL = 3;
    constexpr int SLASH_FILL = 4;
    constexpr int BKSLASH_FILL = 5;
    constexpr int LTBKSLASH_FILL = 6;
    constexpr int HATCH_FILL = 7;
    constexpr int XHATCH_FILL = 8;
    constexpr int INTERLEAVE_FILL = 9;
    constexpr int WIDE_DOT_FILL = 10;
    constexpr int CLOSE_DOT_FILL = 11;
    constexpr int USER_FILL = 12;

    constexpr int COPY_PUT = 0;
    constexpr int XOR_PUT = 1;
    constexpr int OR_PUT = 2;
    constexpr int AND_PUT = 3;
    constexpr int NOT_PUT = 4;

    constexpr int DEFAULT_FONT = 0;
    constexpr int TRIPLEX_FONT = 1;
    constexpr int SMALL_FONT = 2;
    constexpr int SANS_SERIF_FONT = 4;
    constexpr int GOTHIC_FONT = 8;
    constexpr int HORIZ_DIR = 0;
    constexpr int VERT_DIR = 1;

    constexpr int LEFT_TEXT = 0;
    constexpr int CENTER_TEXT = 1;
    constexpr int RIGHT_TEXT = 2;
    constexpr int BOTTOM_TEXT = 0;
    constexpr int TOP_TEXT = 2;

    enum GraphStatus
    {
        grOk = 0,
        grNoInitGraph = -1,
        grNotDetected = -2,
        grFileNotFound = -3,
        grInvalidDriver = -4,
        grNoLoadMem = -5,
        grNoScanMem = -6,
        grNoFloodMem = -7,
        grFontNotFound = -8,
        grNoFontMem = -9,
        grInvalidMode = -10,
        grError = -11,
        grIOerror = -12,
        grInvalidFont = -13,
        grInvalidFontNum = -14,
        grInvalidVersion = -18,
        grInitFailed = -19,
        grWindowClosed = -20,
        grInvalidInput = -21,
    };

    struct arccoordstype
    {
        int x;
        int y;
        int xstart;
        int ystart;
        int xend;
        int yend;
    };

    struct fillsettingstype
    {
        int pattern;
        int color;
    };

    struct linesettingstype
    {
        int linestyle;
        unsigned upattern;
        int thickness;
    };

    struct palettetype
    {
        std::uint8_t size;
        std::array<int, kPaletteSize> colors;
    };

    struct textsettingstype
    {
        int font;
        int direction;
        int charsize;
        int horiz;
        int vert;
    };

    struct viewporttype
    {
        int left;
        int top;
        int right;
        int bottom;
        int clip;
    };

    struct ColorRGB
    {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    using MouseHandler = void(BGI_CALL *)(int, int);

    struct MouseClickEvent
    {
        int kind;
        int x;
        int y;
    };

    struct ImageHeader
    {
        std::uint32_t width;
        std::uint32_t height;
    };

    extern const ColorRGB kBgiPalette[kPaletteSize];

    struct BgiState
    {
        GLFWwindow *window{nullptr};
        int width{kDefaultWidth};
        int height{kDefaultHeight};
        int graphDriver{DETECT};
        int graphMode{0};
        int currentColor{WHITE};
        int bkColor{BLACK};
        int fillPattern{SOLID_FILL};
        int fillColor{WHITE};
        std::array<std::uint8_t, kPatternRows> fillMask{};
        bool userFillPatternEnabled{false};
        int lastResult{grNoInitGraph};
        bool glfwInitialized{false};
        viewporttype viewport{0, 0, kDefaultWidth - 1, kDefaultHeight - 1, 1};
        int currentX{0};
        int currentY{0};
        linesettingstype lineSettings{SOLID_LINE, 0xFFFFU, NORM_WIDTH};
        textsettingstype textSettings{DEFAULT_FONT, HORIZ_DIR, 1, LEFT_TEXT, TOP_TEXT};
        int userCharSizeXNum{1};
        int userCharSizeXDen{1};
        int userCharSizeYNum{1};
        int userCharSizeYDen{1};
        int aspectX{1};
        int aspectY{1};
        int writeMode{COPY_PUT};
        arccoordstype lastArc{0, 0, 0, 0, 0, 0};
        palettetype defaultPalette{};
        palettetype activePalette{};
        std::array<ColorRGB, kPaletteSize> palette{};
        unsigned graphBufSize{0U};
        int activePage{0};
        int visualPage{0};
        std::vector<std::vector<std::uint8_t>> pageBuffers;
        std::string windowTitle{"BGI OpenGL Wrapper"};
        bool doubleBuffered{false};
        std::queue<int> keyQueue;
        int mouseX{0};
        int mouseY{0};
    };

} // namespace bgi
