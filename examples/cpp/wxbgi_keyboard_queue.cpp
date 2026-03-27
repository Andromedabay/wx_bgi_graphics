#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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
}

int main()
{
    if (initwindow(900, 320, "wx_BGI keyboard queue demo", 80, 80, 1, 1) != 0)
    {
        return 1;
    }

    setbkcolor(bgi::BLACK);
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);

    int lastKeyCode = -1;
    int lastExtendedCode = -1;
    bool waitingForExtendedCode = false;

    while (wxbgi_should_close() == 0)
    {
        while (wxbgi_key_pressed() != 0)
        {
            const int keyCode = wxbgi_read_key();
            if (keyCode < 0)
            {
                break;
            }

            if (waitingForExtendedCode)
            {
                lastExtendedCode = keyCode;
                std::cout << "extended scan: " << keyCode << '\n';
                waitingForExtendedCode = false;
            }
            else
            {
                lastKeyCode = keyCode;
                    std::cout << "queued key: " << keyCode << '\n';
                    if (keyCode == 0)
                {
                    waitingForExtendedCode = true;
                }
                else if (keyCode == 27)
                {
                    wxbgi_request_close();
                }
            }
        }

        wxbgi_begin_advanced_frame(0.02f, 0.04f, 0.08f, 1.0f, 1, 0);

        setcolor(bgi::LIGHTGREEN);
        outtextxy(20, 20, const_cast<char *>("Keyboard queue demo"));
        setcolor(bgi::WHITE);
        outtextxy(20, 50, const_cast<char *>("Press normal keys, arrow keys, or Esc to close."));
        outtextxy(20, 70, const_cast<char *>("Extended keys emit 0 first, then a DOS-style scan code."));

        std::string keyLine = "Last queued key: " + describeKeyCode(lastKeyCode);
        std::string extendedLine = "Last extended scan: " + describeKeyCode(lastExtendedCode);
        std::string escDownLine = std::string("Esc currently down: ") + (wxbgi_is_key_down(GLFW_KEY_ESCAPE) == 1 ? "yes" : "no");
        std::string leftDownLine = std::string("Left arrow down: ") + (wxbgi_is_key_down(GLFW_KEY_LEFT) == 1 ? "yes" : "no");

        outtextxy(20, 120, keyLine.data());
        outtextxy(20, 145, extendedLine.data());
        outtextxy(20, 170, escDownLine.data());
        outtextxy(20, 195, leftDownLine.data());

        setcolor(bgi::YELLOW);
        rectangle(12, 12, 880, 235);

        wxbgi_end_advanced_frame(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    closegraph();
    return 0;
}