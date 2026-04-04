/**
 * @file test_input_bypass.cpp
 * @brief Automated test: scroll hook, default-bypass flags, wxbgi_hk_* DDS functions.
 *
 * Build with -DWXBGI_ENABLE_TEST_SEAMS.  Exit code: 0 = pass, 1 = failure.
 */

#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_overlay.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Shared callback state (all static globals — lambda closures not used so
// function pointers with BGI_CALL are always plain static functions)
// ---------------------------------------------------------------------------
static int    g_scrollCalls  = 0;
static double g_scrollLastX  = 0.0;
static double g_scrollLastY  = 0.0;
static int    g_mouseCalls   = 0;
static int    g_keyCalls     = 0;
static int    g_hkMouseX     = -1;
static int    g_hkMouseY     = -1;
static int    g_hkSelCount   = -1;

static void BGI_CALL cbScroll(double x, double y)
{
    ++g_scrollCalls;
    g_scrollLastX = x;
    g_scrollLastY = y;
}

static void BGI_CALL cbScrollHkQuery(double, double)
{
    // Read selection state using hook-context functions
    g_hkSelCount = wxbgi_hk_dds_get_selected_count();
    // Also exercise select/deselect
    if (wxbgi_hk_dds_is_selected("hk_obj_a"))
        wxbgi_hk_dds_deselect("hk_obj_a");
    else
        wxbgi_hk_dds_select("hk_obj_a");
}

static void BGI_CALL cbScrollHkDeselAll(double, double)
{
    wxbgi_hk_dds_deselect_all();
}

static void BGI_CALL cbMouse(int button, int action, int mods)
{
    (void)button; (void)action; (void)mods;
    ++g_mouseCalls;
    g_hkMouseX = wxbgi_hk_get_mouse_x();
    g_hkMouseY = wxbgi_hk_get_mouse_y();
}

static void BGI_CALL cbKey(int, int, int, int) { ++g_keyCalls; }

// ---------------------------------------------------------------------------
// Assertion helpers
// ---------------------------------------------------------------------------
static int g_failures = 0;

static void check(bool cond, const char *msg)
{
    if (cond)
        std::printf("  PASS: %s\n", msg);
    else
    {
        std::fprintf(stderr, "  FAIL: %s\n", msg);
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
#ifdef WXBGI_ENABLE_TEST_SEAMS

static void testScrollHook()
{
    std::printf("\n--- Phase 1: Scroll hook + accumulation ---\n");

    wxbgi_set_scroll_hook(cbScroll);

    g_scrollCalls = 0;
    wxbgi_test_simulate_scroll(1.0, 3.0);
    wxbgi_test_simulate_scroll(-0.5, 2.0);

    check(g_scrollCalls == 2,          "hook called twice");
    check(g_scrollLastX == -0.5,       "last xoffset == -0.5");
    check(g_scrollLastY == 2.0,        "last yoffset == 2.0");

    double dx = 99.0, dy = 99.0;
    wxbgi_get_scroll_delta(&dx, &dy);
    check(dx == 0.5, "accumulated dx == 0.5 (1.0 + -0.5)");
    check(dy == 5.0, "accumulated dy == 5.0 (3.0 + 2.0)");

    wxbgi_get_scroll_delta(&dx, &dy);
    check(dx == 0.0 && dy == 0.0, "delta cleared after read");

    // Remove hook; delta still accumulates
    wxbgi_set_scroll_hook(nullptr);
    g_scrollCalls = 0;
    wxbgi_test_simulate_scroll(7.0, 4.0);
    check(g_scrollCalls == 0, "no hook call after removal");
    wxbgi_get_scroll_delta(&dx, &dy);
    check(dx == 7.0 && dy == 4.0, "delta accumulates without hook");
}

static void testBypassFlags()
{
    std::printf("\n--- Phase 2: Default bypass flags ---\n");

    check(wxbgi_get_input_defaults() == WXBGI_DEFAULT_ALL,
          "initial flags == WXBGI_DEFAULT_ALL");

    // --- KEY_QUEUE bypass ---
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_KEY_QUEUE);
    wxbgi_test_clear_key_queue();
    wxbgi_set_key_hook(cbKey);
    g_keyCalls = 0;
    // ESC is queued by keyCallback when KEY_QUEUE is active; bypassed now
    wxbgi_test_simulate_key(256 /*GLFW_KEY_ESCAPE*/, 1, WXBGI_KEY_PRESS, 0);
    check(g_keyCalls == 1,             "key hook fires even when KEY_QUEUE bypassed");
    check(wxbgi_key_pressed() == 0,    "queue empty when KEY_QUEUE bypassed");
    wxbgi_set_key_hook(nullptr);

    // Re-enable; ESC should be queued
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_test_clear_key_queue();
    wxbgi_test_simulate_key(256, 1, WXBGI_KEY_PRESS, 0);
    check(wxbgi_key_pressed() != 0, "ESC queued when KEY_QUEUE enabled");
    wxbgi_test_clear_key_queue();

    // --- CURSOR_TRACK bypass ---
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_test_simulate_cursor(100, 200);     // set known position
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_CURSOR_TRACK);
    wxbgi_test_simulate_cursor(999, 888);     // should NOT update
    int mx = 0, my = 0;
    wxbgi_get_mouse_pos(&mx, &my);
    check(mx == 100 && my == 200, "mouse pos unchanged when CURSOR_TRACK bypassed");

    // --- SCROLL_ACCUM bypass ---
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_SCROLL_ACCUM);
    double dx = 0.0, dy = 0.0;
    wxbgi_get_scroll_delta(&dx, &dy); // clear any residue
    wxbgi_test_simulate_scroll(5.0, 3.0);
    wxbgi_get_scroll_delta(&dx, &dy);
    check(dx == 0.0 && dy == 0.0, "delta not accumulated when SCROLL_ACCUM bypassed");

    // Scroll hook still fires even when SCROLL_ACCUM bypassed
    wxbgi_set_scroll_hook(cbScroll);
    g_scrollCalls = 0;
    wxbgi_test_simulate_scroll(1.0, 1.0);
    check(g_scrollCalls == 1, "scroll hook still fires when SCROLL_ACCUM bypassed");
    wxbgi_set_scroll_hook(nullptr);

    // Restore
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    check(wxbgi_get_input_defaults() == WXBGI_DEFAULT_ALL, "defaults restored");
}

static void testHookContextDds()
{
    std::printf("\n--- Phase 3: Hook-context DDS functions ---\n");

    // ---- hk_get_mouse_x/y ----
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_test_simulate_cursor(42, 77);
    wxbgi_set_mouse_button_hook(cbMouse);
    g_mouseCalls = 0;
    g_hkMouseX = -1; g_hkMouseY = -1;
    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
    check(g_mouseCalls == 1,    "mouse hook called");
    check(g_hkMouseX == 42,    "hk_get_mouse_x == 42 in hook");
    check(g_hkMouseY == 77,    "hk_get_mouse_y == 77 in hook");
    wxbgi_set_mouse_button_hook(nullptr);

    // ---- hk_dds_select / deselect / is_selected / count ----
    wxbgi_selection_clear();
    // Outside a hook these functions are safe in single-threaded test context
    wxbgi_hk_dds_select("test_obj_1");
    wxbgi_hk_dds_select("test_obj_2");
    check(wxbgi_selection_count() == 2,                  "selected 2 via hk_dds_select");
    check(wxbgi_hk_dds_is_selected("test_obj_1") == 1,   "test_obj_1 is selected");
    check(wxbgi_hk_dds_is_selected("test_obj_2") == 1,   "test_obj_2 is selected");
    check(wxbgi_hk_dds_is_selected("missing") == 0,      "missing obj is not selected");

    // Deselect one
    wxbgi_hk_dds_deselect("test_obj_1");
    check(wxbgi_selection_count() == 1,                  "count == 1 after deselect");
    check(wxbgi_hk_dds_is_selected("test_obj_1") == 0,   "test_obj_1 no longer selected");

    // hk_dds_get_selected_count and get_selected_id
    check(wxbgi_hk_dds_get_selected_count() == 1, "hk_get_selected_count == 1");
    char idbuf[128];
    int r = wxbgi_hk_dds_get_selected_id(0, idbuf, (int)sizeof(idbuf));
    check(r > 0 && std::strcmp(idbuf, "test_obj_2") == 0, "hk_get_selected_id(0) == test_obj_2");
    check(wxbgi_hk_dds_get_selected_id(5, idbuf, (int)sizeof(idbuf)) == -1,
          "hk_get_selected_id out-of-range returns -1");

    // ---- Toggle via scroll hook (fires with mutex held) ----
    // "test_obj_2" is currently selected. cbScrollHkQuery toggles "hk_obj_a" and reads count.
    g_hkSelCount = -1;
    wxbgi_set_scroll_hook(cbScrollHkQuery);
    wxbgi_test_simulate_scroll(0.0, 1.0);   // hk_obj_a not selected → select it
    check(g_hkSelCount == 1,   "selection count was 1 when scroll hook ran");
    check(wxbgi_hk_dds_is_selected("hk_obj_a") == 1, "hk_obj_a added inside scroll hook");

    wxbgi_test_simulate_scroll(0.0, 1.0);   // hk_obj_a now selected → deselect it
    check(wxbgi_hk_dds_is_selected("hk_obj_a") == 0, "hk_obj_a removed inside scroll hook");

    // ---- deselect_all from hook ----
    wxbgi_set_scroll_hook(cbScrollHkDeselAll);
    wxbgi_hk_dds_select("x1");
    wxbgi_hk_dds_select("x2");
    wxbgi_test_simulate_scroll(0.0, 1.0);
    check(wxbgi_selection_count() == 0, "deselect_all from hook clears selection");

    wxbgi_set_scroll_hook(nullptr);
    wxbgi_selection_clear();

    // ---- MOUSE_PICK bypass: hook replaces pick logic ----
    std::printf("\n--- Phase 3b: MOUSE_PICK bypass ---\n");
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_MOUSE_PICK);
    wxbgi_selection_clear();
    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
    // No pick ran; selection should still be empty
    check(wxbgi_selection_count() == 0, "no auto-pick when MOUSE_PICK bypassed");
    // Use hk_dds_select directly to simulate manual pick
    wxbgi_hk_dds_select("manual_pick_obj");
    check(wxbgi_selection_count() == 1, "manual pick via hk_dds_select works");
    check(wxbgi_hk_dds_is_selected("manual_pick_obj") == 1, "manual_pick_obj is selected");

    wxbgi_set_mouse_button_hook(nullptr);
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_selection_clear();
}

#endif // WXBGI_ENABLE_TEST_SEAMS

// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    bool automated = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--simulate") == 0 || std::strcmp(argv[i], "-s") == 0)
            automated = true;

#ifdef WXBGI_ENABLE_TEST_SEAMS
    automated = true;
#endif

    if (!automated)
    {
        std::printf("Run with --simulate for headless automated tests.\n");
        return 0;
    }

#ifndef WXBGI_ENABLE_TEST_SEAMS
    std::printf("WXBGI_ENABLE_TEST_SEAMS not defined — automated tests unavailable.\n");
    return 0;
#else
    std::printf("=== test_input_bypass: automated mode ===\n");

    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(320, 240, "test_input_bypass");
    if (graphresult() != 0)
    {
        std::fprintf(stderr, "wxbgi_wx_frame_create failed\n");
        return 1;
    }

    testScrollHook();
    testBypassFlags();
    testHookContextDds();

    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();

    if (g_failures == 0)
        std::printf("\nAll test_input_bypass tests PASSED.\n");
    else
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);

    return g_failures == 0 ? 0 : 1;
#endif
}
