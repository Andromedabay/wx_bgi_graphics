#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void dummyDriver()
    {
    }

    void require(bool condition, const std::string &message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    void drawCoverageScene()
    {
        setbkcolor(bgi::BLUE);
        setgraphmode(0);
        require(getgraphmode() == 0, "getgraphmode returned unexpected value");

        setviewport(20, 20, 619, 459, 1);
        setcolor(bgi::YELLOW);
        rectangle(0, 0, getmaxx(), getmaxy());

        setlinestyle(bgi::DASHED_LINE, 0xF0F0U, bgi::THICK_WIDTH);
        moveto(20, 20);
        line(20, 20, 180, 60);
        lineto(260, 120);
        linerel(40, 30);
        moverel(-20, 40);
        putpixel(getx(), gety(), bgi::LIGHTMAGENTA);
        require(getpixel(getx(), gety()) >= 0, "getpixel failed after putpixel");

        setlinestyle(bgi::SOLID_LINE, 0xFFFFU, bgi::NORM_WIDTH);
        circle(120, 160, 40);
        arc(120, 160, 30, 240, 60);
        ellipse(260, 150, 0, 300, 60, 30);

        bgi::arccoordstype arcInfo{};
        getarccoords(&arcInfo);
        require(arcInfo.x != 0 || arcInfo.y != 0, "getarccoords did not update state");

        setfillstyle(bgi::HATCH_FILL, bgi::LIGHTRED);
        bar(320, 40, 400, 120);
        bar3d(420, 40, 520, 120, 18, 1);
        fillellipse(480, 180, 48, 28);
        pieslice(120, 300, 20, 160, 55);
        sector(260, 300, 200, 340, 60, 35);

        int polygon[] = {330, 220, 380, 180, 440, 210, 430, 280, 340, 290};
        drawpoly(5, polygon);
        setfillstyle(bgi::XHATCH_FILL, bgi::LIGHTGREEN);
        fillpoly(5, polygon);

        setfillstyle(bgi::SOLID_FILL, bgi::LIGHTCYAN);
        rectangle(40, 340, 140, 420);
        floodfill(80, 380, bgi::YELLOW);

        char customPattern[8] = {static_cast<char>(0x81), static_cast<char>(0x42), static_cast<char>(0x24), static_cast<char>(0x18),
                                 static_cast<char>(0x18), static_cast<char>(0x24), static_cast<char>(0x42), static_cast<char>(0x81)};
        setfillpattern(customPattern, bgi::GREEN);
        bar(170, 340, 250, 420);

        bgi::fillsettingstype fillInfo{};
        getfillsettings(&fillInfo);
        require(fillInfo.color >= 0, "getfillsettings failed");

        char returnedPattern[8] = {};
        getfillpattern(returnedPattern);

        settextstyle(bgi::TRIPLEX_FONT, bgi::HORIZ_DIR, 2);
        settextjustify(bgi::LEFT_TEXT, bgi::TOP_TEXT);
        setusercharsize(2, 1, 2, 1);
        char title[] = "BGI COVERAGE C++";
        outtextxy(20, 20, title);

        settextstyle(bgi::SANS_SERIF_FONT, bgi::VERT_DIR, 1);
        settextjustify(bgi::CENTER_TEXT, bgi::CENTER_TEXT);
        char side[] = "VERT";
        outtextxy(560, 200, side);

        settextstyle(bgi::GOTHIC_FONT, bgi::HORIZ_DIR, 1);
        settextjustify(bgi::LEFT_TEXT, bgi::TOP_TEXT);
        moveto(40, 250);
        char footer[] = "OUTTEXT WIDTH HEIGHT";
        outtext(footer);
        require(textwidth(footer) > 0, "textwidth returned zero");
        require(textheight(footer) > 0, "textheight returned zero");

        unsigned imageBytes = imagesize(320, 40, 400, 120);
        std::vector<unsigned char> image(imageBytes);
        getimage(320, 40, 400, 120, image.data());
        putimage(430, 300, image.data(), bgi::XOR_PUT);

        bgi::linesettingstype lineInfo{};
        gettextsettings(nullptr);
        getlinesettings(&lineInfo);
        require(lineInfo.thickness >= 1, "getlinesettings failed");

        bgi::textsettingstype textInfo{};
        gettextsettings(&textInfo);
        require(textInfo.direction == bgi::HORIZ_DIR, "gettextsettings direction mismatch");

        bgi::viewporttype viewInfo{};
        getviewsettings(&viewInfo);
        require(viewInfo.right > viewInfo.left, "getviewsettings failed");

        int xasp = 0;
        int yasp = 0;
        setaspectratio(4, 3);
        getaspectratio(&xasp, &yasp);
        require(xasp == 4 && yasp == 3, "getaspectratio failed");

        require(getbkcolor() == bgi::BLUE, "getbkcolor mismatch");
        require(getcolor() >= 0, "getcolor failed");
        require(getmaxcolor() == 15, "getmaxcolor mismatch");
        require(getmaxmode() == 0, "getmaxmode mismatch");
        require(getwindowwidth() > 0 && getwindowheight() > 0, "window dimensions invalid");
        require(getmaxwidth() >= getwindowwidth(), "getmaxwidth invalid");
        require(getmaxheight() >= getwindowheight(), "getmaxheight invalid");
        require(std::strlen(getdrivername()) > 0, "getdrivername failed");
        require(std::strlen(getmodename(0)) > 0, "getmodename failed");

        int loMode = -1;
        int hiMode = -1;
        getmoderange(bgi::DETECT, &loMode, &hiMode);
        require(loMode == 0 && hiMode == 0, "getmoderange mismatch");

        auto *defaultPalette = getdefaultpalette();
        require(defaultPalette != nullptr, "getdefaultpalette returned null");

        bgi::palettetype palette{};
        getpalette(&palette);
        require(getpalettesize() == 16, "getpalettesize mismatch");
        setpalette(1, bgi::LIGHTBLUE);
        setrgbpalette(2, 32, 220, 90);
        setallpalette(defaultPalette);

        setactivepage(1);
        require(getactivepage() == 1, "setactivepage failed");
        setvisualpage(0);
        require(getvisualpage() == 0, "setvisualpage failed");
        setcolor(bgi::WHITE);
        outtextxy(30, 30, title);
        require(swapbuffers() == 0, "swapbuffers failed");

        setwritemode(bgi::OR_PUT);
        putpixel(10, 10, bgi::LIGHTGREEN);
        setwritemode(bgi::COPY_PUT);

        clearviewport();
        setviewport(0, 0, getwindowwidth() - 1, getwindowheight() - 1, 1);
        cleardevice();
        graphdefaults();

        char finalText[] = "BGI API COVERAGE OK";
        settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 2);
        outtextxy(30, 30, finalText);

        require(wxbgi_is_ready() == 1, "wxbgi_is_ready failed");
        require(wxbgi_poll_events() == 0, "wxbgi_poll_events failed");
        require(wxbgi_should_close() == 0, "wxbgi_should_close unexpected close flag");
        require(wxbgi_set_window_title("Advanced API Coverage") == 0, "wxbgi_set_window_title failed");
        require(wxbgi_set_vsync(1) == 0, "wxbgi_set_vsync failed");
        require(wxbgi_make_context_current() == 0, "wxbgi_make_context_current failed");

        int windowW = 0;
        int windowH = 0;
        int fbW = 0;
        int fbH = 0;
        require(wxbgi_get_window_size(&windowW, &windowH) == 0, "wxbgi_get_window_size failed");
        require(wxbgi_get_framebuffer_size(&fbW, &fbH) == 0, "wxbgi_get_framebuffer_size failed");
        require(windowW > 0 && windowH > 0 && fbW > 0 && fbH > 0, "advanced size query returned invalid values");

        require(std::strlen(wxbgi_get_gl_string(0)) > 0, "wxbgi_get_gl_string vendor failed");
        require(std::strlen(wxbgi_get_gl_string(1)) > 0, "wxbgi_get_gl_string renderer failed");
        require(std::strlen(wxbgi_get_gl_string(2)) > 0, "wxbgi_get_gl_string version failed");
        require(wxbgi_get_proc_address("glClear") != nullptr, "wxbgi_get_proc_address failed");

        require(wxbgi_begin_advanced_frame(0.08f, 0.10f, 0.14f, 1.0f, 1, 0) == 0, "wxbgi_begin_advanced_frame failed");
        require(wxbgi_end_advanced_frame(1) == 0, "wxbgi_end_advanced_frame failed");

        std::array<unsigned char, 16> writePixels = {
            255, 32, 32, 255,
            32, 255, 32, 255,
            32, 32, 255, 255,
            255, 255, 32, 255};
        require(
            wxbgi_write_pixels_rgba8(1, 1, 2, 2, writePixels.data(), static_cast<int>(writePixels.size())) > 0,
            "wxbgi_write_pixels_rgba8 failed");

        std::vector<unsigned char> pixels(4 * 4 * 4);
        require(wxbgi_read_pixels_rgba8(0, 0, 4, 4, pixels.data(), static_cast<int>(pixels.size())) > 0, "wxbgi_read_pixels_rgba8 failed");
        require(wxbgi_swap_window_buffers() == 0, "wxbgi_swap_window_buffers failed");
        require(wxbgi_get_time_seconds() >= 0.0, "wxbgi_get_time_seconds failed");

        delay(30);
    }
} // namespace

int main()
{
    int driver = -1;
    int mode = -1;

    detectgraph(&driver, &mode);
    require(driver == bgi::DETECT && mode == 0, "detectgraph returned unexpected values");
    require(setgraphbufsize(8192) == 0U, "setgraphbufsize initial value mismatch");
    require(installuserdriver(const_cast<char *>("dummy"), nullptr) == -1, "installuserdriver should be stubbed");
    require(installuserfont(const_cast<char *>("dummy")) == -1, "installuserfont should be stubbed");
    require(registerbgidriver(&dummyDriver) == 0, "registerbgidriver failed");
    require(registerbgifont(&dummyDriver) == 0, "registerbgifont failed");

    // Some CI runners may fail initgraph due to desktop/OpenGL session constraints.
    // Continue with initwindow coverage so the API surface is still validated.
    initgraph(&driver, &mode, nullptr);
    const int initgraphStatus = graphresult();
    if (initgraphStatus == bgi::grOk)
    {
        closegraph();
    }

    require(
        initwindow(640, 480, "BGI Coverage", 80, 80, 1, 1) == 0,
        std::string("initwindow failed (initgraph status=") + std::to_string(initgraphStatus) +
            ": " + grapherrormsg(initgraphStatus) + ")");
    require(graphresult() == bgi::grOk, std::string("initwindow graphresult: ") + grapherrormsg(graphresult()));

    drawCoverageScene();
    restorecrtmode();

    std::cout << "C++ coverage completed" << std::endl;
    return 0;
}