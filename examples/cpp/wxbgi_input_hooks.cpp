// wxbgi_input_hooks.cpp — interactive demo of input hook system
#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include <cstdio>
#include <string>
#include <chrono>

static std::string g_keyLog;
static int g_mouseX = 0, g_mouseY = 0;
static bool g_leftDown = false;

static void BGI_CALL onKey(int key, int scan, int action, int mods)
{
    (void)scan; (void)mods;
    if (action == WXBGI_KEY_PRESS)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "K%d ", key);
        g_keyLog += buf;
        if (g_keyLog.size() > 60) g_keyLog = g_keyLog.substr(g_keyLog.size() - 60);
    }
}

static void BGI_CALL onCursor(double x, double y)
{
    g_mouseX = (int)x;
    g_mouseY = (int)y;
}

static void BGI_CALL onMouseBtn(int btn, int action, int mods)
{
    (void)mods;
    if (btn == WXBGI_MOUSE_LEFT) g_leftDown = (action == WXBGI_KEY_PRESS);
}

int main(void)
{
    initwindow(640, 480, "Input Hooks Demo", 100, 100, 1, 0);
    wxbgi_set_key_hook(onKey);
    wxbgi_set_cursor_pos_hook(onCursor);
    wxbgi_set_mouse_button_hook(onMouseBtn);

    auto start = std::chrono::steady_clock::now();
    while (!wxbgi_should_close())
    {
        wxbgi_poll_events();
        cleardevice();
        setcolor(bgi::WHITE);
        outtextxy(10, 10, const_cast<char*>("Input Hooks Demo -- press keys or move mouse"));
        outtextxy(10, 30, const_cast<char*>(("Keys: " + g_keyLog).c_str()));
        char buf[64];
        snprintf(buf, sizeof(buf), "Mouse: (%d, %d) %s", g_mouseX, g_mouseY, g_leftDown ? "[LEFT]" : "");
        outtextxy(10, 50, buf);
        swapbuffers();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= 5) break;
    }
    closegraph();
    return 0;
}
