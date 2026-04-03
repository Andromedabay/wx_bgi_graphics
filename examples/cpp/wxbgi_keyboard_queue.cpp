#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include "bgi_types.h"

#include <iostream>
#include <string>

namespace
{
    std::string describeKeyCode(int keyCode)
    {
        if (keyCode < 0)
        {
            return "none";
        }

        if (keyCode == 0)
        {
            return "0 (extended prefix)";
        }

        if (keyCode >= 32 && keyCode <= 126)
        {
            return std::string(1, static_cast<char>(keyCode)) + " (" + std::to_string(keyCode) + ")";
        }

        return std::to_string(keyCode);
    }

    static int lastKeyCode         = -1;
    static int lastExtendedCode    = -1;
    static bool waitingForExtended = false;

    static void BGI_CALL drawFrame()
    {
        while (wxbgi_key_pressed() != 0)
        {
            const int keyCode = wxbgi_read_key();
            if (keyCode < 0)
                break;

            if (waitingForExtended)
            {
                lastExtendedCode = keyCode;
                std::cout << "extended scan: " << keyCode << '\n';
                waitingForExtended = false;
            }
            else
            {
                lastKeyCode = keyCode;
                std::cout << "queued key: " << keyCode << '\n';
                if (keyCode == 0)
                    waitingForExtended = true;
                else if (keyCode == 27)
                    wxbgi_wx_close_frame();
            }
        }

        cleardevice();

        setcolor(bgi::LIGHTGREEN);
        outtextxy(20, 20, const_cast<char *>("Keyboard queue demo"));
        setcolor(bgi::WHITE);
        outtextxy(20, 50, const_cast<char *>("Press normal keys, arrow keys, or Esc to close."));
        outtextxy(20, 70, const_cast<char *>("Extended keys emit 0 first, then a DOS-style scan code."));

        std::string keyLine      = "Last queued key: "    + describeKeyCode(lastKeyCode);
        std::string extLine      = "Last extended scan: " + describeKeyCode(lastExtendedCode);
        std::string escDownLine  = std::string("Esc currently down: ")  + (wxbgi_is_key_down(WXBGI_KEY_ESCAPE) == 1 ? "yes" : "no");
        std::string leftDownLine = std::string("Left arrow down: ")     + (wxbgi_is_key_down(WXBGI_KEY_LEFT)   == 1 ? "yes" : "no");

        outtextxy(20, 120, keyLine.data());
        outtextxy(20, 145, extLine.data());
        outtextxy(20, 170, escDownLine.data());
        outtextxy(20, 195, leftDownLine.data());

        setcolor(bgi::YELLOW);
        rectangle(12, 12, 880, 235);

        swapbuffers();
    }
}

int main()
{
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(900, 320, "wx_BGI keyboard queue demo");

    setbkcolor(bgi::BLACK);
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);

    wxbgi_wx_set_idle_callback(drawFrame);
    wxbgi_wx_set_frame_rate(60);
    wxbgi_wx_app_main_loop();
    return 0;
}