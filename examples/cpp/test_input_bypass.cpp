// test_input_bypass.cpp — automated bypass flags + scroll + hk_* test
// Requires WXBGI_ENABLE_TEST_SEAMS.
#ifndef WXBGI_ENABLE_TEST_SEAMS
#  error "This test requires -DWXBGI_ENABLE_TEST_SEAMS=1"
#endif

#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)

// ----- Phase 1: Scroll hook -----
static double g_scrollX = 0.0, g_scrollY = 0.0;
static int g_scrollCalled = 0;
static void BGI_CALL scrollHook(double x, double y) { g_scrollX = x; g_scrollY = y; g_scrollCalled++; }

// ----- Phase 2: Bypass flags -----
static int g_keyHookCalled = 0;
static void BGI_CALL bypassKeyHook(int key, int scan, int action, int mods)
{ (void)key; (void)scan; (void)action; (void)mods; g_keyHookCalled++; }

static double g_cursorHookX = -1.0;
static void BGI_CALL bypassCursorHook(double x, double y) { g_cursorHookX = x; (void)y; }

int main(void)
{
    initwindow(320, 240, "test_input_bypass", 0, 0, 0, 0);
    CHECK(wxbgi_is_ready());

    // --- Phase 1: Scroll hook ---
    wxbgi_set_scroll_hook(scrollHook);
    wxbgi_test_simulate_scroll(0.0, 1.0);
    CHECK(g_scrollCalled == 1);
    CHECK(g_scrollY == 1.0);

    double dx = -1, dy = -1;
    wxbgi_get_scroll_delta(&dx, &dy);
    CHECK(dy == 1.0);  // accumulated
    // After read, delta should be cleared
    wxbgi_get_scroll_delta(&dx, &dy);
    CHECK(dy == 0.0);

    // WXBGI_DEFAULT_SCROLL_ACCUM off: hook still fires, delta not accumulated
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_SCROLL_ACCUM);
    g_scrollCalled = 0;
    wxbgi_test_simulate_scroll(0.0, 5.0);
    CHECK(g_scrollCalled == 1);   // hook still fires
    wxbgi_get_scroll_delta(&dx, &dy);
    CHECK(dy == 0.0);             // not accumulated
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_set_scroll_hook(nullptr);

    // --- Phase 2: Bypass key queue ---
    wxbgi_set_key_hook(bypassKeyHook);
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_KEY_QUEUE);
    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
    CHECK(g_keyHookCalled == 1);     // hook fires
    CHECK(!wxbgi_key_pressed());     // queue empty (bypass)
    CHECK(wxbgi_is_key_down(65) == 1); // keyDown always updated

    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
    CHECK(wxbgi_is_key_down(65) == 0);
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_set_key_hook(nullptr);

    // --- Bypass cursor track ---
    wxbgi_set_cursor_pos_hook(bypassCursorHook);
    // Set baseline position with all defaults ON
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_test_simulate_cursor(50.0, 60.0);
    int bx, by;
    wxbgi_get_mouse_pos(&bx, &by);
    CHECK(bx == 50 && by == 60);

    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_CURSOR_TRACK);
    g_cursorHookX = -1.0;
    wxbgi_test_simulate_cursor(99.0, 88.0);
    CHECK(g_cursorHookX == 99.0);   // hook fires
    wxbgi_get_mouse_pos(&bx, &by);
    CHECK(bx == 50 && by == 60);    // position NOT updated
    wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
    wxbgi_set_cursor_pos_hook(nullptr);

    // --- Phase 3: hk_* functions ---
    int selCount = wxbgi_hk_dds_get_selected_count();
    CHECK(selCount == 0);

    wxbgi_hk_dds_select("obj1");
    wxbgi_hk_dds_select("obj2");
    CHECK(wxbgi_hk_dds_get_selected_count() == 2);
    CHECK(wxbgi_hk_dds_is_selected("obj1") == 1);
    CHECK(wxbgi_hk_dds_is_selected("obj3") == 0);

    char buf[64];
    int len = wxbgi_hk_dds_get_selected_id(0, buf, sizeof(buf));
    CHECK(len >= 0);
    CHECK(std::string(buf) == "obj1");

    wxbgi_hk_dds_deselect("obj1");
    CHECK(wxbgi_hk_dds_get_selected_count() == 1);
    CHECK(wxbgi_hk_dds_is_selected("obj1") == 0);

    wxbgi_hk_dds_deselect_all();
    CHECK(wxbgi_hk_dds_get_selected_count() == 0);

    // hk_get_mouse_x/y
    wxbgi_test_simulate_cursor(77.0, 88.0);
    CHECK(wxbgi_hk_get_mouse_x() == 77);
    CHECK(wxbgi_hk_get_mouse_y() == 88);

    closegraph();
    printf("test_input_bypass: PASS\n");
    return 0;
}
