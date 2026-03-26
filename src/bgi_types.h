#pragma once

#include <array>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// DLL export / calling-convention macros
// ---------------------------------------------------------------------------
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

// Forward-declare opaque GLFW window handle so headers that use BgiState
// do not need to pull in the full GLFW header.
struct GLFWwindow;

namespace bgi
{

    // ---------------------------------------------------------------------------
    // Default window dimensions
    // ---------------------------------------------------------------------------
    constexpr int kDefaultWidth = 960;
    constexpr int kDefaultHeight = 720;

    // ---------------------------------------------------------------------------
    // Error codes returned by graphresult()
    // ---------------------------------------------------------------------------
    enum GraphStatus
    {
        grOk = 0,
        grNoInitGraph = -1,
        grInitFailed = -2,
        grWindowClosed = -3,
        grInvalidInput = -4,
    };

    // ---------------------------------------------------------------------------
    // 24-bit RGB colour value used internally
    // ---------------------------------------------------------------------------
    struct ColorRGB
    {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    // 16-entry standard BGI palette; defined in bgi_state.cpp
    extern const ColorRGB kBgiPalette[16];

    // ---------------------------------------------------------------------------
    // All mutable rendering state lives here
    // ---------------------------------------------------------------------------
    struct BgiState
    {
        GLFWwindow *window{nullptr};
        int width{kDefaultWidth};
        int height{kDefaultHeight};
        int currentColor{15};
        int fillPattern{1};
        int fillColor{15};
        int lastResult{grNoInitGraph};
        bool glfwInitialized{false};

        std::vector<std::uint8_t> colorIndexBuffer;
        std::vector<std::uint8_t> rgbaBuffer;
    };

} // namespace bgi
