#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
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

/**
 * @defgroup wxbgi_input_hooks User Input Hook Callbacks
 * @brief Optional user-supplied callbacks invoked after the library's own
 *        internal event processing.
 *
 * Register hooks with `wxbgi_set_key_hook()`, `wxbgi_set_char_hook()`,
 * `wxbgi_set_cursor_pos_hook()`, and `wxbgi_set_mouse_button_hook()`.
 * Pass `NULL` to any registration function to remove the hook.
 *
 * @warning Hooks fire while the library's internal mutex is held.  They
 *          must **not** call any `wxbgi_*` function — doing so will deadlock.
 *          Safe operations: update your own variables, set flags, call
 *          non-wxbgi code.
 * @{
 */

/**
 * @brief User hook fired after each key press, repeat, or release event.
 *
 * Parameters mirror the GLFW key callback: @p key is the GLFW key constant
 * (e.g. `GLFW_KEY_ESCAPE`), @p scancode is the raw OS scancode, @p action is
 * one of `WXBGI_KEY_PRESS` / `WXBGI_KEY_RELEASE` / `WXBGI_KEY_REPEAT`, and
 * @p mods is a bitfield of `WXBGI_MOD_SHIFT`, `WXBGI_MOD_CTRL`, `WXBGI_MOD_ALT`.
 */
typedef void (BGI_CALL *WxbgiKeyHook)(int key, int scancode, int action, int mods);

/**
 * @brief User hook fired after each printable Unicode character input event.
 *
 * @p codepoint is the Unicode code point (1–255 used by this library; values
 * outside that range are silently dropped before the hook is invoked).
 */
typedef void (BGI_CALL *WxbgiCharHook)(unsigned int codepoint);

/**
 * @brief User hook fired after every mouse cursor movement.
 *
 * @p x and @p y are the new cursor position in window pixels, origin at
 * the top-left corner of the client area.
 */
typedef void (BGI_CALL *WxbgiCursorPosHook)(int x, int y);

/**
 * @brief User hook fired after each mouse button press or release.
 *
 * @p button is `WXBGI_MOUSE_LEFT`, `WXBGI_MOUSE_RIGHT`, or
 * `WXBGI_MOUSE_MIDDLE`. @p action is `WXBGI_KEY_PRESS` or
 * `WXBGI_KEY_RELEASE`. @p mods is a bitfield of modifier keys.
 */
typedef void (BGI_CALL *WxbgiMouseButtonHook)(int button, int action, int mods);

/**
 * @brief User hook fired after each mouse scroll (wheel) event.
 *
 * @p xoffset is the horizontal scroll delta; @p yoffset is the vertical
 * scroll delta.  Positive @p yoffset means scroll up/forward; negative
 * means scroll down/backward.  Parameters mirror the GLFW scroll callback.
 */
typedef void (BGI_CALL *WxbgiScrollHook)(double xoffset, double yoffset);

/** @} */

/**
 * @name Input default-behavior flags
 * Bitmask constants for `wxbgi_set_input_defaults()`.
 * Combine with bitwise OR; pass `WXBGI_DEFAULT_ALL` to restore all defaults.
 * @{
 */
/** Key-code queuing: keyCallback updates keyQueue + charCallback queues chars. */
#define WXBGI_DEFAULT_KEY_QUEUE    0x01
/** Cursor tracking: cursorPosCallback writes mouseX / mouseY / mouseMoved. */
#define WXBGI_DEFAULT_CURSOR_TRACK 0x02
/** Mouse-button picking: mouseButtonCallback calls overlayPerformPick(). */
#define WXBGI_DEFAULT_MOUSE_PICK   0x04
/** Scroll accumulation: scrollCallback accumulates scrollDeltaX / scrollDeltaY. */
#define WXBGI_DEFAULT_SCROLL_ACCUM 0x08
/** All default behaviors enabled (initial state after initwindow). */
#define WXBGI_DEFAULT_ALL          0x0F
/** All default behaviors disabled. */
#define WXBGI_DEFAULT_NONE         0x00
/** @} */

/**
 * @name Navigation and function key codes
 * Named constants for keyboard keys, matching GLFW values for hook callback
 * compatibility. Use with `wxbgi_is_key_down()` and hook callbacks.
 * @{
 */
#define WXBGI_KEY_SPACE         32
#define WXBGI_KEY_APOSTROPHE    39
#define WXBGI_KEY_COMMA         44
#define WXBGI_KEY_MINUS         45
#define WXBGI_KEY_PERIOD        46
#define WXBGI_KEY_SLASH         47
#define WXBGI_KEY_SEMICOLON     59
#define WXBGI_KEY_EQUAL         61
#define WXBGI_KEY_LEFT_BRACKET  91
#define WXBGI_KEY_RIGHT_BRACKET 93
#define WXBGI_KEY_ESCAPE        256
#define WXBGI_KEY_ENTER         257
#define WXBGI_KEY_TAB           258
#define WXBGI_KEY_BACKSPACE     259
#define WXBGI_KEY_INSERT        260
#define WXBGI_KEY_DELETE        261
#define WXBGI_KEY_RIGHT         262
#define WXBGI_KEY_LEFT          263
#define WXBGI_KEY_DOWN          264
#define WXBGI_KEY_UP            265
#define WXBGI_KEY_PAGE_UP       266
#define WXBGI_KEY_PAGE_DOWN     267
#define WXBGI_KEY_HOME          268
#define WXBGI_KEY_END           269
#define WXBGI_KEY_F1            290
#define WXBGI_KEY_F2            291
#define WXBGI_KEY_F3            292
#define WXBGI_KEY_F4            293
#define WXBGI_KEY_F5            294
#define WXBGI_KEY_F6            295
#define WXBGI_KEY_F7            296
#define WXBGI_KEY_F8            297
#define WXBGI_KEY_F9            298
#define WXBGI_KEY_F10           299
#define WXBGI_KEY_F11           300
#define WXBGI_KEY_F12           301
/** @} */

namespace bgi
{

    constexpr int DETECT = 0;
    constexpr int kDefaultWidth = 960;
    constexpr int kDefaultHeight = 720;
    constexpr int kPaletteSize    = 16;   ///< classic BGI palette slots (0-15)
    constexpr int kExtColorBase   = 16;   ///< first user-assignable extended colour index
    constexpr int kExtPaletteSize = 240;  ///< extended slots 16-255 (fits uint8_t pixel buffer)
    constexpr int kSelectionOrangeColor = 252;  ///< Reserved extended-palette slot — selection flash orange.
    constexpr int kSelectionPurpleColor = 253;  ///< Reserved extended-palette slot — selection flash purple.
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
        grDuplicateName = -22, ///< A camera or UCS with that name already exists.
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

    // -------------------------------------------------------------------------
    // Camera system
    // -------------------------------------------------------------------------

    /** Projection mode for a Camera3D. */
    enum class CameraProjection
    {
        Orthographic = 0,
        Perspective  = 1,
    };

    /**
     * @brief 3-D camera definition (Z-up, right-handed world coordinate system).
     *
     * The camera stores both full 3-D eye/target/up parameters and a 2-D
     * convenience layer (pan/zoom/rotation) for overhead/plan-view cameras.
     * When @c is2D is true, the 2-D fields drive the view and projection
     * matrices; otherwise the explicit 3-D and ortho fields are used.
     *
     * All camera instances live in @c BgiState::cameras keyed by name.
     * A camera named @c "default" is created automatically on @c initwindow()
     * and replicates the classic BGI pixel-space coordinate system.
     */
    struct Camera3D
    {
        // --- Eye/target/up (Z-up, right-handed) ---
        float eyeX{0.f},    eyeY{-10.f},  eyeZ{5.f};
        float targetX{0.f}, targetY{0.f}, targetZ{0.f};
        float upX{0.f},     upY{0.f},     upZ{1.f};  // Z is up

        // --- Projection ---
        CameraProjection projection{CameraProjection::Orthographic};

        float fovYDeg{45.f};
        float nearPlane{0.1f};
        float farPlane{10000.f};

        // Orthographic clip extents. All-zero triggers auto-fit via worldHeight2d.
        float orthoLeft{0.f},   orthoRight{0.f};
        float orthoBottom{0.f}, orthoTop{0.f};

        // --- Screen-space viewport override (pixels).
        //     All-zero means the full GLFW window. ---
        int vpX{0}, vpY{0}, vpW{0}, vpH{0};

        // --- 2-D convenience layer (active when is2D == true) ---
        bool  is2D{false};
        float pan2dX{0.f}, pan2dY{0.f};
        float zoom2d{1.f};
        float rot2dDeg{0.f};
        /** Visible world-units height used by 2-D cameras and auto-ortho. */
        float worldHeight2d{2.f};

        // --- Per-camera visual overlay state (not serialised to DDS/DDJ) ---

        /// Concentric circles + crosshair overlay.
        struct {
            bool  enabled     {false};
            int   count       {3};       ///< number of circles (1..8)
            float innerRadius {25.f};    ///< world-unit radius of innermost circle
            float outerRadius {100.f};   ///< world-unit radius of outermost circle
        } concentricOverlay;

        /// Selection cursor square overlay (moves with the mouse in this viewport).
        struct {
            bool  enabled     {false};
            int   sizePx      {8};       ///< square side length, pixels (2..16)
            int   colorScheme {0};       ///< 0 = blue, 1 = green, 2 = red
        } selCursorOverlay;
    };

    // -------------------------------------------------------------------------
    // User Coordinate System (UCS)
    // -------------------------------------------------------------------------

    /**
     * @brief A named user coordinate system (UCS).
     *
     * A UCS defines a local frame in world space through an origin point and
     * three orthonormal axes.  The axes must form a right-handed system.
     *
     * A UCS named @c "world" (the identity frame) is created automatically on
     * @c initwindow() / @c initgraph() and cannot be destroyed.  Classic BGI
     * drawing is unaffected by the UCS system.
     */
    struct CoordSystem
    {
        // Origin of this UCS in world space.
        float originX{0.f}, originY{0.f}, originZ{0.f};

        // Orthonormal axes expressed as world-space direction vectors.
        // These default to the world identity frame (Z-up, right-handed).
        float xAxisX{1.f}, xAxisY{0.f}, xAxisZ{0.f};
        float yAxisX{0.f}, yAxisY{1.f}, yAxisZ{0.f};
        float zAxisX{0.f}, zAxisY{0.f}, zAxisZ{1.f};
    };

    // -------------------------------------------------------------------------
    // Visual overlay state
    // -------------------------------------------------------------------------

    /**
     * @brief State for the reference grid overlay.
     *
     * The grid is drawn in the XY plane of the active (or specified) UCS and
     * projected through each camera that renders the scene.  It is NOT a DDS
     * object and cannot be selected or serialised.
     */
    struct OverlayGridState
    {
        bool        enabled    {false};
        float       spacing    {25.f};   ///< world units between adjacent grid lines
        int         halfExtent {4};      ///< grid spans ±(halfExtent×spacing) from UCS origin
        int         xAxisColor {RED};    ///< colour for the line coincident with UCS local X axis
        int         yAxisColor {GREEN};  ///< colour for the line coincident with UCS local Y axis
        int         gridColor  {DARKGRAY}; ///< colour for all non-axis grid lines
        std::string ucsName;             ///< which UCS to use; empty = BgiState::activeUcs
    };

    /**
     * @brief State for the UCS axes overlay.
     *
     * Draws labelled X/Y/Z axis arms at the world origin (world UCS) and/or at
     * the active UCS origin, projected through every camera viewport.
     */
    struct OverlayUcsAxesState
    {
        bool  enabled    {false};
        bool  showWorld  {true};   ///< draw world UCS axes at (0,0,0)
        bool  showActive {true};   ///< draw active UCS axes at its own origin
        float axisLength {80.f};   ///< world-unit length from origin to axis tip
    };

    // -------------------------------------------------------------------------
    // World extents
    // -------------------------------------------------------------------------

    /**
     * @brief Axis-aligned bounding box (AABB) representing the programmer's
     *        declared drawing extents in world space.
     *
     * Used by @c wxbgi_cam_fit_to_extents() and as a reference extent for
     * viewport calculations.  @c hasData is @c false until the first call to
     * @c wxbgi_set_world_extents() or @c wxbgi_expand_world_extents().
     */
    struct WorldExtents
    {
        float minX{0.f}, minY{0.f}, minZ{0.f};
        float maxX{0.f}, maxY{0.f}, maxZ{0.f};
        bool  hasData{false};
    };

    /**
     * @brief Lighting parameters used by the GL Phong shading passes.
     *
     * Primary light direction is normalised by the API setter.
     * All intensities are in [0,1]; shininess is a Phong exponent (e.g. 8..256).
     */
    struct LightState
    {
        // Primary (key) light direction — normalised, world-space by default.
        float dirX{ -0.577f}, dirY{ 0.577f}, dirZ{0.577f};
        bool  worldSpace{true};          ///< true = world-space, false = view-space

        // Fill (secondary) light — softens shadows.
        float fillX{0.f}, fillY{-1.f}, fillZ{0.f};
        float fillStrength{0.30f};

        // Phong material intensities.
        float ambient  {0.20f};
        float diffuse  {0.70f};
        float specular {0.30f};
        float shininess{32.f};
    };

    // -------------------------------------------------------------------------
    // Forward declarations for DDS types (full definitions in bgi_dds.h).
    // These let BgiState hold shared_ptr<DdsCamera> and unique_ptr<DdsScene>
    // without creating a circular include between bgi_types.h and bgi_dds.h.
    // -------------------------------------------------------------------------
    class DdsCamera;
    class DdsUcs;
    class DdsScene;

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
        std::array<ColorRGB, kPaletteSize>    palette{};
        std::array<ColorRGB, kExtPaletteSize> extPalette{};  ///< user-assigned RGB slots 16-255
        int extColorNext{kExtColorBase};                      ///< next auto-alloc slot
        unsigned graphBufSize{0U};
        int activePage{0};
        int visualPage{0};
        std::vector<std::vector<std::uint8_t>> pageBuffers;
        std::string windowTitle{"BGI OpenGL Wrapper"};
        bool doubleBuffered{false};
        std::queue<int> keyQueue;
        std::array<std::uint8_t, 512> keyDown{};
        int mouseX{0};
        int mouseY{0};
        bool mouseMoved{false}; ///< Set true by cursorPosCallback; cleared by wxbgi_mouse_moved().

        // --- Camera registry (DdsCamera is the source of truth) ---
        // shared_ptr<DdsCamera> maps are indexes into the DDS; Camera3D data
        // lives inside each DdsCamera object.  Forward declaration keeps this
        // header free of the full bgi_dds.h include.
        std::unordered_map<std::string, std::shared_ptr<DdsCamera>> cameras;
        std::string activeCamera{"default"};

        // --- UCS registry (DdsUcs is the source of truth) ---
        std::unordered_map<std::string, std::shared_ptr<DdsUcs>> ucsSystems;
        std::string activeUcs{"world"};

        // --- World drawing extents (AABB in world space) ---
        WorldExtents worldExtents;

        // --- Drawing Description Data Structure (DDS) scene graph ---
        // unique_ptr so BgiState can be defined in this header without the
        // full DdsScene definition.  Initialized in BgiState::BgiState() and
        // reset in resetStateForWindow() (both defined in bgi_state.cpp where
        // bgi_dds.h is included).
        std::unique_ptr<DdsScene> dds;

        // --- 3D Solid / Surface / Extrusion defaults ---
        int solidDrawMode{0};         ///< 0 = Wireframe, 1 = Solid (painter's algorithm).
        int solidEdgeColor{15};       ///< Default edge colour (WHITE).
        int solidFaceColor{7};        ///< Default face colour (LIGHTGRAY).
        int solidColorOverride{-1};   ///< When >= 0, replaces faceColor and edgeColor in renderTriangleBatch (used by selection flash).

        // --- Global visual overlays (not serialised) ---
        OverlayGridState    overlayGrid;
        OverlayUcsAxesState overlayUcsAxes;

        // --- DDS Object Selection state (not serialised) ---
        std::vector<std::string> selectedObjectIds;   ///< IDs of currently selected DDS drawing objects.
        int selectionFlashScheme {0};                 ///< Flash colour: 0 = orange (252), 1 = purple (253).
        int selectionPickRadiusPx{16};                ///< Screen-pixel pick threshold for object selection.

        // --- User input hooks ---
        WxbgiKeyHook          userKeyHook{nullptr};
        WxbgiCharHook         userCharHook{nullptr};
        WxbgiCursorPosHook   userCursorPosHook{nullptr}; ///< Called after cursorPosCallback logic; may be NULL.
        WxbgiMouseButtonHook  userMouseButtonHook{nullptr};
        WxbgiScrollHook       userScrollHook{nullptr};
        double scrollDeltaX{0.0}; ///< Accumulated horizontal scroll delta.
        double scrollDeltaY{0.0}; ///< Accumulated vertical scroll delta.

        // --- Input default behavior flags ---
        int inputDefaultFlags{WXBGI_DEFAULT_ALL}; ///< Bitmask controlling built-in callback behaviors.

        // --- wx-embedded mode flag ---
        bool wxEmbedded{false}; ///< True when the BGI surface is hosted inside a WxBgiCanvas.
        bool shouldClose{false}; ///< In wx mode: set by wxbgi_request_close / frame close event.
        bool inCrtMode{false};  ///< Set by restorecrtmode(); cleared by setgraphmode(). Window stays open.
        /// Optional callback set by the standalone frame to pump wx events.
        /// Called by wxbgi_poll_events() WITHOUT holding gMutex so wx handlers
        /// can safely call back into the BGI API.
        void (*wxPollCallback)(){nullptr};

        // --- GL rendering mode ---
        bool legacyGlRender{false}; ///< When true, uses the old GL_POINTS per-pixel path.
        LightState lightState;       ///< Lighting parameters for GL Phong shading passes.

        // Explicitly declared(defined in bgi_state.cpp) so that the compiler
        // generates the destructor only where DdsScene is fully defined.
        BgiState();
        ~BgiState();
        BgiState(const BgiState &) = delete;
        BgiState &operator=(const BgiState &) = delete;
    };

} // namespace bgi
