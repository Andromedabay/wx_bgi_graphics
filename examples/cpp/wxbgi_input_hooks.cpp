/**
 * @file wxbgi_input_hooks.cpp
 * @brief Interactive demo: user input hook callbacks.
 *
 * Demonstrates all four hook types firing in real time:
 *
 *  - **Key hook**         — status panel shows last GLFW key + action;
 *                           arrow keys move a red crosshair marker around
 *                           the canvas; 'C' cycles the stamp colour.
 *  - **Char hook**        — typed printable characters appear in a text
 *                           area at the bottom; Backspace removes the last.
 *  - **Cursor pos hook**  — a yellow crosshair follows the mouse cursor;
 *                           each move updates the status panel.
 *  - **Mouse button hook**— left-click stamps a filled circle at the
 *                           cursor; right-click clears all stamps.
 *
 * Status panels across the top show the last event and fire count for each
 * hook type.  Press Escape or close the window to exit.
 */

#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
// Application state — written by hooks, read by the render loop
// (all accesses are on the same thread; no extra locking needed)
// ---------------------------------------------------------------------------
struct StatusPanel
{
    const char *label;
    std::string last;
    int         count{0};
    int         color;
};

static StatusPanel gKeyPanel   {"KEY  HOOK", {}, 0, bgi::LIGHTCYAN};
static StatusPanel gCharPanel  {"CHAR HOOK", {}, 0, bgi::LIGHTGREEN};
static StatusPanel gCursorPanel{"CURSOR   ", {}, 0, bgi::YELLOW};
static StatusPanel gMousePanel {"MOUSE    ", {}, 0, bgi::LIGHTMAGENTA};

static int gCursorX{400}, gCursorY{290};   // mouse cursor (cursor hook)
static int gMarkerX{400}, gMarkerY{290};   // keyboard-controlled marker

static std::vector<std::array<int, 3>> gStamps; // {x, y, color}
static int gStampColor{bgi::CYAN};

static std::string gTyped;
static constexpr int kMaxTyped = 42;

static char gFmtBuf[128]; // scratch buffer for formatting inside hooks

// Colour cycle for 'C' key
static const int kColors[] = {
    bgi::CYAN, bgi::YELLOW, bgi::LIGHTRED,
    bgi::LIGHTGREEN, bgi::LIGHTMAGENTA, bgi::WHITE
};
static int gColorIndex{0};

// ---------------------------------------------------------------------------
// Hook callbacks — must NOT call any wxbgi_* (mutex is held)
// ---------------------------------------------------------------------------
static void BGI_CALL hookKey(int key, int /*scancode*/, int action, int mods)
{
    if (action != WXBGI_KEY_RELEASE)
        ++gKeyPanel.count;
    const char *act = (action == WXBGI_KEY_PRESS)   ? "PRESS"
                    : (action == WXBGI_KEY_REPEAT)  ? "RPT"
                    :                                  "REL";
    std::snprintf(gFmtBuf, sizeof(gFmtBuf), "key=%-3d %s mods=%d", key, act, mods);
    gKeyPanel.last = gFmtBuf;

    if (action == WXBGI_KEY_RELEASE)
        return;

    // Arrow keys: move marker
    constexpr int kStep = 8;
    if      (key == GLFW_KEY_LEFT)  gMarkerX -= kStep;
    else if (key == GLFW_KEY_RIGHT) gMarkerX += kStep;
    else if (key == GLFW_KEY_UP)    gMarkerY -= kStep;
    else if (key == GLFW_KEY_DOWN)  gMarkerY += kStep;
    // 'C': cycle stamp colour
    else if (key == GLFW_KEY_C && action == WXBGI_KEY_PRESS)
    {
        gColorIndex = (gColorIndex + 1) % static_cast<int>(sizeof(kColors) / sizeof(kColors[0]));
        gStampColor = kColors[gColorIndex];
    }
}

static void BGI_CALL hookChar(unsigned int cp)
{
    ++gCharPanel.count;
    std::snprintf(gFmtBuf, sizeof(gFmtBuf), "'%c' (%u)",
                  (cp >= 32 && cp <= 126) ? static_cast<char>(cp) : '?', cp);
    gCharPanel.last = gFmtBuf;

    if (cp == 8u && !gTyped.empty())
        gTyped.pop_back();
    else if (cp >= 32u && cp <= 126u && static_cast<int>(gTyped.size()) < kMaxTyped)
        gTyped += static_cast<char>(cp);
}

static void BGI_CALL hookCursor(int x, int y)
{
    ++gCursorPanel.count;
    std::snprintf(gFmtBuf, sizeof(gFmtBuf), "(%d, %d)", x, y);
    gCursorPanel.last = gFmtBuf;
    gCursorX = x;
    gCursorY = y;
}

static void BGI_CALL hookMouse(int button, int action, int mods)
{
    ++gMousePanel.count;
    const char *bname = (button == WXBGI_MOUSE_LEFT)  ? "LEFT"
                      : (button == WXBGI_MOUSE_RIGHT) ? "RIGHT"
                      :                                  "MID";
    const char *act   = (action == WXBGI_KEY_PRESS) ? "PRESS" : "REL";
    std::snprintf(gFmtBuf, sizeof(gFmtBuf), "%s %s m=%d", bname, act, mods);
    gMousePanel.last = gFmtBuf;

    if (button == WXBGI_MOUSE_LEFT && action == WXBGI_KEY_PRESS)
        gStamps.push_back({gCursorX, gCursorY, gStampColor});
    else if (button == WXBGI_MOUSE_RIGHT && action == WXBGI_KEY_PRESS)
        gStamps.clear();
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------
static void drawCrosshair(int x, int y, int r, int col)
{
    setcolor(col);
    line(x - r, y,     x + r, y);
    line(x,     y - r, x,     y + r);
    circle(x, y, r);
}

static void drawPanel(int px, int py, int pw, int ph, const StatusPanel &s)
{
    setcolor(s.color);
    rectangle(px, py, px + pw, py + ph);
    outtextxy(px + 5, py + 5,  const_cast<char *>(s.label));
    setcolor(bgi::WHITE);
    std::string last = s.last.empty() ? "(none)" : s.last;
    outtextxy(px + 5, py + 20, const_cast<char *>(last.c_str()));
    std::string cnt  = "n=" + std::to_string(s.count);
    setcolor(s.color);
    outtextxy(px + pw - 45, py + 5, const_cast<char *>(cnt.c_str()));
}

} // namespace

int main()
{
    constexpr int kW = 900, kH = 580;
    if (initwindow(kW, kH, "wx_BGI input hooks demo — Esc to exit", 80, 80, 1, 1) != 0)
        return 1;

    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);

    wxbgi_set_key_hook(hookKey);
    wxbgi_set_char_hook(hookChar);
    wxbgi_set_cursor_pos_hook(hookCursor);
    wxbgi_set_mouse_button_hook(hookMouse);

    constexpr int kPH     = 52;
    const     int kPW     = (kW - 24) / 4;
    constexpr int kPY     = 4;
    constexpr int kDrawY  = kPY + kPH + 14;

    while (wxbgi_should_close() == 0)
    {
        wxbgi_poll_events();

        // Drain the queue so Escape closes the window
        while (wxbgi_key_pressed() != 0)
        {
            if (wxbgi_read_key() == 27)
                wxbgi_request_close();
        }

        wxbgi_begin_advanced_frame(0.03f, 0.03f, 0.07f, 1.0f, 1, 0);

        // Status panels
        drawPanel(4,              kPY, kPW, kPH, gKeyPanel);
        drawPanel(8  + kPW,       kPY, kPW, kPH, gCharPanel);
        drawPanel(12 + kPW * 2,   kPY, kPW, kPH, gCursorPanel);
        drawPanel(16 + kPW * 3,   kPY, kPW, kPH, gMousePanel);

        // Instructions
        setcolor(bgi::LIGHTGRAY);
        outtextxy(6, kDrawY,
            const_cast<char *>(
                "Arrow keys = move red marker | C = cycle colour | "
                "Left-click = stamp | Right-click = clear | type chars"));

        // Stamps
        for (const auto &s : gStamps)
        {
            setfillstyle(bgi::SOLID_FILL, s[2]);
            setcolor(s[2]);
            fillellipse(s[0], s[1], 14, 14);
        }

        // Keyboard-controlled marker (red crosshair)
        drawCrosshair(gMarkerX, gMarkerY, 12, bgi::LIGHTRED);

        // Mouse cursor crosshair (yellow)
        drawCrosshair(gCursorX, gCursorY, 12, bgi::YELLOW);

        // Typed text area
        constexpr int kTextY = kH - 34;
        setcolor(bgi::CYAN);
        rectangle(4, kTextY - 4, kW - 4, kH - 6);
        setcolor(bgi::WHITE);
        std::string tdisp = "Typed: " + gTyped;
        outtextxy(10, kTextY, const_cast<char *>(tdisp.c_str()));

        wxbgi_end_advanced_frame(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    closegraph();
    return 0;
}
