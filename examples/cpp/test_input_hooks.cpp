/**
 * @file test_input_hooks.cpp
 * @brief Automated test: user input hook callbacks (`wxbgi_set_*_hook` API).
 *
 * **Phase 1** (always compiled): verifies that all four hook registration
 * functions and NULL-deregistration complete without crashing.
 *
 * **Phase 2** (requires `WXBGI_ENABLE_TEST_SEAMS`): uses
 * `wxbgi_test_simulate_key/char/cursor/mouse_button` to drive the full
 * callback → user-hook pipeline without real OS input. Checks:
 *   - Each hook fires the expected number of times.
 *   - Hook parameters match the simulated event values.
 *   - `gState` is already updated before the hook returns:
 *       * `wxbgi_is_key_down` reflects the new press / release state.
 *       * `wxbgi_get_mouse_pos` returns the simulated cursor position.
 *       * `wxbgi_mouse_moved` returns 1 after cursor simulation.
 *       * `wxbgi_key_pressed` / `wxbgi_read_key` reflect queued chars.
 *   - Setting a hook to NULL stops it from firing.
 *
 * **Phase 3** (always): draws summary primitives to confirm the BGI drawing
 * pipeline is still functional after all hook operations.
 *
 * Exits 0 on success, 1 on any assertion failure.
 */

#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#include <cstdio>
#include <cstdlib>

namespace
{

// ---------------------------------------------------------------------------
// Hook data: written from hook context, read from main
// ---------------------------------------------------------------------------
struct KeyRecord
{
    int count{0};
    int key{-1}, scancode{-1}, action{-1}, mods{-1};
};
struct CharRecord
{
    int          count{0};
    unsigned int codepoint{0};
};
struct CursorRecord
{
    int count{0}, x{-1}, y{-1};
};
struct MouseRecord
{
    int count{0}, button{-1}, action{-1}, mods{-1};
};

static KeyRecord    gKey;
static CharRecord   gChar;
static CursorRecord gCursor;
static MouseRecord  gMouse;

// ---------------------------------------------------------------------------
// Hook callbacks — do NOT call any wxbgi_* from here: mutex is held
// ---------------------------------------------------------------------------
static void BGI_CALL hookKey(int key, int scancode, int action, int mods)
{
    ++gKey.count;
    gKey.key      = key;
    gKey.scancode = scancode;
    gKey.action   = action;
    gKey.mods     = mods;
}

static void BGI_CALL hookChar(unsigned int codepoint)
{
    ++gChar.count;
    gChar.codepoint = codepoint;
}

static void BGI_CALL hookCursor(int x, int y)
{
    ++gCursor.count;
    gCursor.x = x;
    gCursor.y = y;
}

static void BGI_CALL hookMouse(int button, int action, int mods)
{
    ++gMouse.count;
    gMouse.button = button;
    gMouse.action = action;
    gMouse.mods   = mods;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_input_hooks]: %s\n", msg);
    std::exit(1);
}

void require(bool cond, const char *msg)
{
    if (!cond)
        fail(msg);
}

} // namespace

int main()
{
    constexpr int kW = 400, kH = 260;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_input_hooks");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    // -----------------------------------------------------------------------
    // Phase 1: registration / deregistration (no TEST_SEAMS required)
    // -----------------------------------------------------------------------
    wxbgi_set_key_hook(hookKey);
    wxbgi_set_char_hook(hookChar);
    wxbgi_set_cursor_pos_hook(hookCursor);
    wxbgi_set_mouse_button_hook(hookMouse);

    // Deregister with NULL — must not crash
    wxbgi_set_key_hook(nullptr);
    wxbgi_set_char_hook(nullptr);
    wxbgi_set_cursor_pos_hook(nullptr);
    wxbgi_set_mouse_button_hook(nullptr);

    // Re-register for simulation phase
    wxbgi_set_key_hook(hookKey);
    wxbgi_set_char_hook(hookChar);
    wxbgi_set_cursor_pos_hook(hookCursor);
    wxbgi_set_mouse_button_hook(hookMouse);

#ifdef WXBGI_ENABLE_TEST_SEAMS
    // -----------------------------------------------------------------------
    // Phase 2: full pipeline simulation
    // -----------------------------------------------------------------------

    // --- 2a. Key hook (GLFW_KEY_A = 65) ---
    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
    require(gKey.count  == 1,               "key hook: not called on press");
    require(gKey.key    == 65,              "key hook: key value wrong");
    require(gKey.action == WXBGI_KEY_PRESS, "key hook: press action wrong");
    require(gKey.mods   == 0,              "key hook: mods wrong");
    require(wxbgi_is_key_down(65) == 1,    "key hook: keyDown[65] not set after press");

    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_REPEAT, 0);
    require(gKey.count  == 2,                "key hook: not called on repeat");
    require(gKey.action == WXBGI_KEY_REPEAT, "key hook: repeat action wrong");
    require(wxbgi_is_key_down(65) == 1,      "key hook: keyDown cleared on repeat");

    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
    require(gKey.count  == 3,                  "key hook: not called on release");
    require(gKey.action == WXBGI_KEY_RELEASE,  "key hook: release action wrong");
    require(wxbgi_is_key_down(65) == 0,        "key hook: keyDown not cleared after release");

    // Special key: Up arrow (GLFW_KEY_UP = 265) should push {0,72} to queue
    wxbgi_test_simulate_key(265, 72, WXBGI_KEY_PRESS, 0);
    require(gKey.count == 4,                  "key hook: not called for Up arrow");
    require(wxbgi_key_pressed() == 1,         "key hook: Up arrow extended prefix not queued");
    require(wxbgi_read_key() == 0,            "key hook: Up arrow prefix byte wrong");
    require(wxbgi_read_key() == 72,           "key hook: Up arrow scancode wrong");

    // --- 2b. Char hook ---
    wxbgi_test_simulate_char(static_cast<unsigned int>('H')); // 72
    require(gChar.count     == 1,  "char hook: not called");
    require(gChar.codepoint == static_cast<unsigned int>('H'),
            "char hook: codepoint wrong");
    require(wxbgi_key_pressed() == 1,           "char hook: char not queued");
    require(wxbgi_read_key() == static_cast<int>('H'),
            "char hook: queued char wrong");

    // Filtered codepoints must NOT call hook and must NOT queue
    const int prevCharCount = gChar.count;
    wxbgi_test_simulate_char(27u); // Escape — filtered out entirely
    require(gChar.count == prevCharCount, "char hook: ESC should not fire hook");
    require(wxbgi_key_pressed() == 0,   "char hook: ESC must not be queued");

    wxbgi_test_simulate_char(300u); // out-of-range — filtered out
    require(gChar.count == prevCharCount, "char hook: out-of-range should not fire hook");

    // --- 2c. Cursor hook ---
    wxbgi_test_simulate_cursor(150, 200);
    require(gCursor.count == 1,   "cursor hook: not called");
    require(gCursor.x     == 150, "cursor hook: x wrong");
    require(gCursor.y     == 200, "cursor hook: y wrong");

    int mx = 0, my = 0;
    wxbgi_get_mouse_pos(&mx, &my);
    require(mx == 150 && my == 200, "cursor hook: gState.mouseX/Y not updated before hook returned");
    require(wxbgi_mouse_moved() == 1, "cursor hook: gState.mouseMoved not set");

    wxbgi_test_simulate_cursor(100, 80);
    require(gCursor.count == 2,                        "cursor hook: not called on second move");
    require(gCursor.x == 100 && gCursor.y == 80,       "cursor hook: second move coords wrong");

    // --- 2d. Mouse button hook ---
    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
    require(gMouse.count  == 1,               "mouse btn hook: not called on left press");
    require(gMouse.button == WXBGI_MOUSE_LEFT, "mouse btn hook: button wrong");
    require(gMouse.action == WXBGI_KEY_PRESS,  "mouse btn hook: action wrong");
    require(gMouse.mods   == 0,               "mouse btn hook: mods wrong");

    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_RIGHT, WXBGI_KEY_PRESS, WXBGI_MOD_CTRL);
    require(gMouse.count  == 2,                "mouse btn hook: not called on right press");
    require(gMouse.button == WXBGI_MOUSE_RIGHT, "mouse btn hook: right button wrong");
    require(gMouse.mods   == WXBGI_MOD_CTRL,    "mouse btn hook: Ctrl mod wrong");

    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_RELEASE, 0);
    require(gMouse.count  == 3,                 "mouse btn hook: not called on release");
    require(gMouse.action == WXBGI_KEY_RELEASE,  "mouse btn hook: release action wrong");

    // --- 2e. Deregistration: hook must stop firing after NULL ---
    const int keyCountBefore = gKey.count;
    wxbgi_set_key_hook(nullptr);
    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
    require(gKey.count == keyCountBefore,
            "key hook: still fires after null deregistration");

    const int mouseCountBefore = gMouse.count;
    wxbgi_set_mouse_button_hook(nullptr);
    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
    require(gMouse.count == mouseCountBefore,
            "mouse btn hook: still fires after null deregistration");

#endif // WXBGI_ENABLE_TEST_SEAMS

    // -----------------------------------------------------------------------
    // Phase 3: draw summary primitives (confirms drawing still works)
    // -----------------------------------------------------------------------
    setbkcolor(bgi::BLACK);
    setgraphmode(0);
    cleardevice();
    setcolor(bgi::WHITE);
    outtextxy(10, 10, const_cast<char *>("test_input_hooks: PASS"));

#ifdef WXBGI_ENABLE_TEST_SEAMS
    // Circle at last simulated cursor position
    setcolor(bgi::YELLOW);
    circle(gCursor.x, gCursor.y, 20);
    // Bars proportional to hook fire counts
    setcolor(bgi::CYAN);
    rectangle(10, 40, 10 + gKey.count * 10, 55);
    setcolor(bgi::GREEN);
    rectangle(10, 60, 10 + gCursor.count * 20, 75);
    setcolor(bgi::LIGHTRED);
    rectangle(10, 80, 10 + gMouse.count * 20, 95);
#else
    setcolor(bgi::YELLOW);
    outtextxy(10, 30, const_cast<char *>("Rebuild with WXBGI_ENABLE_TEST_SEAMS for full test"));
    circle(200, 130, 40);
#endif

    wxbgi_poll_events();
    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_input_hooks]\n");
    return 0;
}
