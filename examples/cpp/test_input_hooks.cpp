// test_input_hooks.cpp — automated hook regression test
// Requires WXBGI_ENABLE_TEST_SEAMS to be defined at compile time.
#ifndef WXBGI_ENABLE_TEST_SEAMS
#  error "This test requires -DWXBGI_ENABLE_TEST_SEAMS=1"
#endif

#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static int g_keyCalled = 0;
static int g_lastKey = -1;
static int g_cursorCalled = 0;
static int g_lastCursorX = -1;
static int g_lastCursorY = -1;
static int g_mouseBtnCalled = 0;
static int g_lastBtn = -1;
static int g_charCalled = 0;

static void BGI_CALL myKeyHook(int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;
    if (action == WXBGI_KEY_PRESS) { g_keyCalled++; g_lastKey = key; }
}

static void BGI_CALL myCursorHook(double x, double y)
{
    g_cursorCalled++;
    g_lastCursorX = (int)x;
    g_lastCursorY = (int)y;
}

static void BGI_CALL myMouseBtnHook(int btn, int action, int mods)
{
    (void)mods;
    if (action == WXBGI_KEY_PRESS) { g_mouseBtnCalled++; g_lastBtn = btn; }
}

static void BGI_CALL myCharHook(unsigned int cp) { g_charCalled++; (void)cp; }

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)

int main(void)
{
    initwindow(320, 240, "test_input_hooks", 0, 0, 0, 0);
    CHECK(wxbgi_is_ready());

    // Register hooks
    wxbgi_set_key_hook(myKeyHook);
    wxbgi_set_cursor_pos_hook(myCursorHook);
    wxbgi_set_mouse_button_hook(myMouseBtnHook);
    wxbgi_set_char_hook(myCharHook);

    // Simulate key press (GLFW_KEY_A = 65)
    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
    CHECK(g_keyCalled == 1);
    CHECK(g_lastKey == 65);
    // keyDown should be set
    CHECK(wxbgi_is_key_down(65) == 1);

    // Simulate key release
    wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
    CHECK(wxbgi_is_key_down(65) == 0);

    // Simulate char
    wxbgi_test_simulate_char('H');
    CHECK(g_charCalled == 1);
    CHECK(wxbgi_key_pressed());
    int k = wxbgi_read_key();
    CHECK(k == 'H');

    // Simulate cursor movement
    wxbgi_test_simulate_cursor(100.0, 200.0);
    CHECK(g_cursorCalled == 1);
    CHECK(g_lastCursorX == 100);
    CHECK(g_lastCursorY == 200);
    int mx = -1, my = -1;
    wxbgi_get_mouse_pos(&mx, &my);
    CHECK(mx == 100 && my == 200);
    CHECK(wxbgi_mouse_moved() == 1); // clears flag
    CHECK(wxbgi_mouse_moved() == 0); // flag was cleared

    // Simulate mouse button
    wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
    CHECK(g_mouseBtnCalled == 1);
    CHECK(g_lastBtn == WXBGI_MOUSE_LEFT);

    // Remove hooks
    wxbgi_set_key_hook(nullptr);
    wxbgi_set_cursor_pos_hook(nullptr);
    wxbgi_set_mouse_button_hook(nullptr);
    wxbgi_set_char_hook(nullptr);

    closegraph();
    printf("test_input_hooks: PASS\n");
    return 0;
}
