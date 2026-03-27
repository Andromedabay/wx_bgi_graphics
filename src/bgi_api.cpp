// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "wx_bgi.h"

#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_image.h"
#include "bgi_state.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace
{
    void queueKeyCode(int keyCode)
    {
        bgi::gState.keyQueue.push(keyCode);
    }

    void queueExtendedKey(int scanCode)
    {
        queueKeyCode(0);
        queueKeyCode(scanCode);
    }

    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        (void)window;
        (void)scancode;
        (void)mods;

        if (key >= 0 && key < static_cast<int>(bgi::gState.keyDown.size()))
        {
            bgi::gState.keyDown[static_cast<std::size_t>(key)] = action != GLFW_RELEASE ? 1U : 0U;
        }

        if (action != GLFW_PRESS && action != GLFW_REPEAT)
        {
            return;
        }

        switch (key)
        {
        case GLFW_KEY_ESCAPE:
            queueKeyCode(27);
            break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            queueKeyCode(13);
            break;
        case GLFW_KEY_TAB:
            queueKeyCode(9);
            break;
        case GLFW_KEY_BACKSPACE:
            queueKeyCode(8);
            break;
        case GLFW_KEY_UP:
            queueExtendedKey(72);
            break;
        case GLFW_KEY_DOWN:
            queueExtendedKey(80);
            break;
        case GLFW_KEY_LEFT:
            queueExtendedKey(75);
            break;
        case GLFW_KEY_RIGHT:
            queueExtendedKey(77);
            break;
        case GLFW_KEY_HOME:
            queueExtendedKey(71);
            break;
        case GLFW_KEY_END:
            queueExtendedKey(79);
            break;
        case GLFW_KEY_PAGE_UP:
            queueExtendedKey(73);
            break;
        case GLFW_KEY_PAGE_DOWN:
            queueExtendedKey(81);
            break;
        case GLFW_KEY_INSERT:
            queueExtendedKey(82);
            break;
        case GLFW_KEY_DELETE:
            queueExtendedKey(83);
            break;
        case GLFW_KEY_F1:
            queueExtendedKey(59);
            break;
        case GLFW_KEY_F2:
            queueExtendedKey(60);
            break;
        case GLFW_KEY_F3:
            queueExtendedKey(61);
            break;
        case GLFW_KEY_F4:
            queueExtendedKey(62);
            break;
        case GLFW_KEY_F5:
            queueExtendedKey(63);
            break;
        case GLFW_KEY_F6:
            queueExtendedKey(64);
            break;
        case GLFW_KEY_F7:
            queueExtendedKey(65);
            break;
        case GLFW_KEY_F8:
            queueExtendedKey(66);
            break;
        case GLFW_KEY_F9:
            queueExtendedKey(67);
            break;
        case GLFW_KEY_F10:
            queueExtendedKey(68);
            break;
        case GLFW_KEY_F11:
            queueExtendedKey(133);
            break;
        case GLFW_KEY_F12:
            queueExtendedKey(134);
            break;
        default:
            break;
        }
    }

    void charCallback(GLFWwindow *window, unsigned int codepoint)
    {
        (void)window;
        if (codepoint <= 0U || codepoint > 255U)
        {
            return;
        }

        if (codepoint == 9U || codepoint == 13U || codepoint == 27U)
        {
            return;
        }

        queueKeyCode(static_cast<int>(codepoint));
    }

    bool initializeWindow(int width, int height, const char *title, int left, int top, bool doubleBuffered)
    {
        if (!bgi::gState.glfwInitialized)
        {
            if (glfwInit() == GLFW_FALSE)
            {
                bgi::gState.lastResult = bgi::grInitFailed;
                return false;
            }
            bgi::gState.glfwInitialized = true;
        }

        bgi::destroyWindowIfNeeded();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        bgi::gState.windowTitle = (title != nullptr && *title != '\0') ? title : "BGI OpenGL Wrapper";
        bgi::gState.window = glfwCreateWindow(width, height, bgi::gState.windowTitle.c_str(), nullptr, nullptr);
        if (bgi::gState.window == nullptr)
        {
            bgi::gState.lastResult = bgi::grInitFailed;
            return false;
        }

        glfwMakeContextCurrent(bgi::gState.window);
        glfwSwapInterval(1);
        glfwSetWindowPos(bgi::gState.window, left, top);

#if !defined(__APPLE__)
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK)
        {
            bgi::destroyWindowIfNeeded();
            bgi::gState.lastResult = bgi::grInitFailed;
            return false;
        }
#endif

        bgi::resetStateForWindow(width, height, doubleBuffered);
        glfwSetKeyCallback(bgi::gState.window, keyCallback);
        glfwSetCharCallback(bgi::gState.window, charCallback);
        bgi::gState.lastResult = bgi::grOk;
        bgi::flushToScreen();
        return true;
    }

    bool requireReady()
    {
        return bgi::isReady();
    }

    std::string safeText(char *textstring)
    {
        return textstring == nullptr ? std::string() : std::string(textstring);
    }

    std::vector<std::pair<int, int>> polygonFromArray(int numpoints, const int *polypoints)
    {
        std::vector<std::pair<int, int>> points;
        if (numpoints <= 0 || polypoints == nullptr)
        {
            return points;
        }

        points.reserve(static_cast<std::size_t>(numpoints));
        for (int index = 0; index < numpoints; ++index)
        {
            points.emplace_back(polypoints[index * 2], polypoints[index * 2 + 1]);
        }
        return points;
    }

    void updateArcState(int centerX, int centerY, const std::vector<std::pair<int, int>> &arcPoints)
    {
        if (arcPoints.empty())
        {
            return;
        }

        bgi::gState.lastArc.x = centerX;
        bgi::gState.lastArc.y = centerY;
        bgi::gState.lastArc.xstart = arcPoints.front().first;
        bgi::gState.lastArc.ystart = arcPoints.front().second;
        bgi::gState.lastArc.xend = arcPoints.back().first;
        bgi::gState.lastArc.yend = arcPoints.back().second;
    }

    void drawTextAnchored(int x, int y, const std::string &text)
    {
        const auto [width, height] = bgi::measureText(text);
        int drawX = x;
        int drawY = y;

        switch (bgi::gState.textSettings.horiz)
        {
        case bgi::CENTER_TEXT:
            drawX -= width / 2;
            break;
        case bgi::RIGHT_TEXT:
            drawX -= width;
            break;
        default:
            break;
        }

        switch (bgi::gState.textSettings.vert)
        {
        case bgi::CENTER_TEXT:
            drawY -= height / 2;
            break;
        case bgi::BOTTOM_TEXT:
            drawY -= height;
            break;
        default:
            break;
        }

        bgi::drawText(drawX, drawY, text, bgi::gState.currentColor);
    }

    void flushIfVisible()
    {
        bgi::flushToScreen();
    }

    void fillSectorShape(int x, int y, int stangle, int endangle, int xradius, int yradius, bool includeCenter)
    {
        auto arcPoints = bgi::buildArcPoints(x, y, stangle, endangle, xradius, yradius);
        updateArcState(x, y, arcPoints);

        std::vector<std::pair<int, int>> polygon;
        if (includeCenter)
        {
            polygon.emplace_back(x, y);
        }
        polygon.insert(polygon.end(), arcPoints.begin(), arcPoints.end());
        if (includeCenter)
        {
            polygon.emplace_back(x, y);
        }

        bgi::fillPolygonInternal(polygon, bgi::gState.fillColor);
        if (includeCenter)
        {
            bgi::drawLineInternal(x, y, arcPoints.front().first, arcPoints.front().second, bgi::gState.currentColor);
            bgi::drawLineInternal(x, y, arcPoints.back().first, arcPoints.back().second, bgi::gState.currentColor);
        }
        bgi::drawEllipseInternal(x, y, stangle, endangle, xradius, yradius, bgi::gState.currentColor);
    }
} // namespace

BGI_API void BGI_CALL arc(int x, int y, int stangle, int endangle, int radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || radius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    const auto points = bgi::buildArcPoints(x, y, stangle, endangle, radius, radius);
    updateArcState(x, y, points);
    bgi::drawEllipseInternal(x, y, stangle, endangle, radius, radius, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL bar(int left, int top, int right, int bottom)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }
    bgi::fillRectInternal(left, top, right, bottom, bgi::gState.fillColor);
    flushIfVisible();
}

BGI_API void BGI_CALL bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    bgi::fillRectInternal(left, top, right, bottom, bgi::gState.fillColor);
    bgi::drawLineInternal(left, top, right, top, bgi::gState.currentColor);
    bgi::drawLineInternal(right, top, right, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(right, bottom, left, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(left, bottom, left, top, bgi::gState.currentColor);
    bgi::drawLineInternal(right, top, right + depth, top - depth, bgi::gState.currentColor);
    bgi::drawLineInternal(right, bottom, right + depth, bottom - depth, bgi::gState.currentColor);
    bgi::drawLineInternal(right + depth, top - depth, right + depth, bottom - depth, bgi::gState.currentColor);
    if (topflag != 0)
    {
        bgi::drawLineInternal(left, top, left + depth, top - depth, bgi::gState.currentColor);
        bgi::drawLineInternal(left + depth, top - depth, right + depth, top - depth, bgi::gState.currentColor);
    }
    flushIfVisible();
}

BGI_API void BGI_CALL circle(int x, int y, int radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || radius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::drawCircleInternal(x, y, radius, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL cleardevice(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    bgi::clearActivePage(bgi::gState.bkColor);
    bgi::gState.currentX = 0;
    bgi::gState.currentY = 0;
    flushIfVisible();
}

BGI_API void BGI_CALL clearviewport(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    bgi::clearViewportRegion(bgi::gState.bkColor);
    bgi::gState.currentX = 0;
    bgi::gState.currentY = 0;
    flushIfVisible();
}

BGI_API void BGI_CALL closegraph(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::destroyWindowIfNeeded();
    bgi::gState.pageBuffers.clear();
    if (bgi::gState.glfwInitialized)
    {
        glfwTerminate();
        bgi::gState.glfwInitialized = false;
    }
    bgi::gState.lastResult = bgi::grNoInitGraph;
}

BGI_API void BGI_CALL delay(int millisec)
{
    if (millisec <= 0)
    {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
}

BGI_API void BGI_CALL detectgraph(int *graphdriver, int *graphmode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (graphdriver != nullptr)
    {
        *graphdriver = bgi::DETECT;
    }
    if (graphmode != nullptr)
    {
        *graphmode = 0;
    }
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL drawpoly(int numpoints, const int *polypoints)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    const auto points = polygonFromArray(numpoints, polypoints);
    if (points.size() < 2)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::drawPolygonInternal(points, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL ellipse(int x, int y, int stangle, int endangle, int xradius, int yradius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || xradius < 0 || yradius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    const auto points = bgi::buildArcPoints(x, y, stangle, endangle, xradius, yradius);
    updateArcState(x, y, points);
    bgi::drawEllipseInternal(x, y, stangle, endangle, xradius, yradius, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL fillellipse(int x, int y, int xradius, int yradius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || xradius < 0 || yradius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::fillEllipseInternal(x, y, xradius, yradius, bgi::gState.fillColor);
    bgi::drawEllipseInternal(x, y, 0, 360, xradius, yradius, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL fillpoly(int numpoints, const int *polypoints)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    const auto points = polygonFromArray(numpoints, polypoints);
    if (points.size() < 3)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::fillPolygonInternal(points, bgi::gState.fillColor);
    bgi::drawPolygonInternal(points, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL floodfill(int x, int y, int border)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    const int borderColor = bgi::normalizeColor(border);
    const int targetColor = bgi::getPixel(x, y);
    if (targetColor < 0 || targetColor == borderColor)
    {
        bgi::gState.lastResult = bgi::grOk;
        return;
    }

    std::queue<std::pair<int, int>> work;
    work.push({x, y});

    while (!work.empty())
    {
        const auto [cx, cy] = work.front();
        work.pop();

        const int current = bgi::getPixel(cx, cy);
        if (current != targetColor || current == borderColor)
        {
            continue;
        }

        bgi::setPixelWithMode(cx, cy, bgi::useFillAt(cx, cy) ? bgi::gState.fillColor : bgi::gState.currentColor, bgi::COPY_PUT);
        work.push({cx + 1, cy});
        work.push({cx - 1, cy});
        work.push({cx, cy + 1});
        work.push({cx, cy - 1});
    }

    flushIfVisible();
}

BGI_API int BGI_CALL getactivepage(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.activePage;
}

BGI_API void BGI_CALL getarccoords(bgi::arccoordstype *arccoords)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (arccoords != nullptr)
    {
        *arccoords = bgi::gState.lastArc;
    }
}

BGI_API void BGI_CALL getaspectratio(int *xasp, int *yasp)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (xasp != nullptr)
    {
        *xasp = bgi::gState.aspectX;
    }
    if (yasp != nullptr)
    {
        *yasp = bgi::gState.aspectY;
    }
}

BGI_API int BGI_CALL getbkcolor(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.bkColor;
}

BGI_API int BGI_CALL getcolor(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.currentColor;
}

BGI_API bgi::palettetype *BGI_CALL getdefaultpalette(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return &bgi::gState.defaultPalette;
}

BGI_API char *BGI_CALL getdrivername(void)
{
    static char name[] = "OPENGL";
    return name;
}

BGI_API void BGI_CALL getfillpattern(char *pattern)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (pattern != nullptr)
    {
        std::memcpy(pattern, bgi::gState.fillMask.data(), bgi::kPatternRows);
    }
}

BGI_API void BGI_CALL getfillsettings(bgi::fillsettingstype *fillinfo)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (fillinfo != nullptr)
    {
        fillinfo->pattern = bgi::gState.fillPattern;
        fillinfo->color = bgi::gState.fillColor;
    }
}

BGI_API int BGI_CALL getgraphmode(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.graphMode;
}

BGI_API void BGI_CALL getimage(int left, int top, int right, int bottom, void *bitmap)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || !bgi::captureImage(left, top, right, bottom, bitmap))
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
    }
}

BGI_API void BGI_CALL getlinesettings(bgi::linesettingstype *lineinfo)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (lineinfo != nullptr)
    {
        *lineinfo = bgi::gState.lineSettings;
    }
}

BGI_API int BGI_CALL getmaxcolor(void)
{
    return bgi::kPaletteSize - 1;
}

BGI_API int BGI_CALL getmaxheight(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.height;
}

BGI_API int BGI_CALL getmaxmode(void)
{
    return 0;
}

BGI_API int BGI_CALL getmaxwidth(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.width;
}

BGI_API int BGI_CALL getmaxx(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return std::max(0, bgi::gState.viewport.right - bgi::gState.viewport.left);
}

BGI_API int BGI_CALL getmaxy(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return std::max(0, bgi::gState.viewport.bottom - bgi::gState.viewport.top);
}

BGI_API char *BGI_CALL getmodename(int mode_number)
{
    static char defaultMode[] = "BGI_OPENGL";
    static char invalidMode[] = "INVALID";
    return mode_number == 0 ? defaultMode : invalidMode;
}

BGI_API void BGI_CALL getmoderange(int graphdriver, int *lomode, int *himode)
{
    (void)graphdriver;
    if (lomode != nullptr)
    {
        *lomode = 0;
    }
    if (himode != nullptr)
    {
        *himode = 0;
    }
}

BGI_API void BGI_CALL getpalette(bgi::palettetype *palette)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (palette != nullptr)
    {
        *palette = bgi::gState.activePalette;
    }
}

BGI_API int BGI_CALL getpalettesize(void)
{
    return bgi::kPaletteSize;
}

BGI_API int BGI_CALL getpixel(int x, int y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return -1;
    }
    return bgi::getPixel(x, y);
}

BGI_API void BGI_CALL gettextsettings(bgi::textsettingstype *texttypeinfo)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (texttypeinfo != nullptr)
    {
        *texttypeinfo = bgi::gState.textSettings;
    }
}

BGI_API void BGI_CALL getviewsettings(bgi::viewporttype *viewport)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (viewport != nullptr)
    {
        *viewport = bgi::gState.viewport;
    }
}

BGI_API int BGI_CALL getvisualpage(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.visualPage;
}

BGI_API int BGI_CALL getwindowheight(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.height;
}

BGI_API int BGI_CALL getwindowwidth(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.width;
}

BGI_API int BGI_CALL getx(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.currentX;
}

BGI_API int BGI_CALL gety(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.currentY;
}

BGI_API void BGI_CALL graphdefaults(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    const int width = bgi::gState.width;
    const int height = bgi::gState.height;
    const bool doubleBuffered = bgi::gState.doubleBuffered;
    bgi::resetStateForWindow(width, height, doubleBuffered);
    flushIfVisible();
}

BGI_API char *BGI_CALL grapherrormsg(int errorcode)
{
    switch (errorcode)
    {
    case bgi::grOk:
        return const_cast<char *>("No error");
    case bgi::grNoInitGraph:
        return const_cast<char *>("Graphics not initialized");
    case bgi::grInitFailed:
        return const_cast<char *>("Graphics initialization failed");
    case bgi::grWindowClosed:
        return const_cast<char *>("Graphics window closed");
    case bgi::grInvalidInput:
        return const_cast<char *>("Invalid graphics input");
    default:
        return const_cast<char *>("Graphics error");
    }
}

BGI_API int BGI_CALL graphresult(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.lastResult;
}

BGI_API unsigned BGI_CALL imagesize(int left, int top, int right, int bottom)
{
    return bgi::imageSizeForRect(left, top, right, bottom);
}

BGI_API void BGI_CALL initgraph(int *graphdriver, int *graphmode, char *pathtodriver)
{
    (void)pathtodriver;
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (graphdriver != nullptr)
    {
        *graphdriver = bgi::DETECT;
    }
    if (graphmode != nullptr)
    {
        *graphmode = 0;
    }

    initializeWindow(bgi::kDefaultWidth, bgi::kDefaultHeight, bgi::gState.windowTitle.c_str(), 64, 64, false);
}

BGI_API int BGI_CALL initwindow(int width, int height, const char *title, int left, int top, int dbflag, int closeflag)
{
    (void)closeflag;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return initializeWindow(std::max(1, width), std::max(1, height), title, left, top, dbflag != 0) ? 0 : -1;
}

BGI_API int BGI_CALL installuserdriver(char *name, void *detect)
{
    (void)name;
    (void)detect;
    return -1;
}

BGI_API int BGI_CALL installuserfont(char *name)
{
    (void)name;
    return -1;
}

BGI_API void BGI_CALL line(int x1, int y1, int x2, int y2)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }
    bgi::drawLineInternal(x1, y1, x2, y2, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API void BGI_CALL linerel(int dx, int dy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }
    const int targetX = bgi::gState.currentX + dx;
    const int targetY = bgi::gState.currentY + dy;
    bgi::drawLineInternal(bgi::gState.currentX, bgi::gState.currentY, targetX, targetY, bgi::gState.currentColor);
    bgi::gState.currentX = targetX;
    bgi::gState.currentY = targetY;
    flushIfVisible();
}

BGI_API void BGI_CALL lineto(int x, int y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }
    bgi::drawLineInternal(bgi::gState.currentX, bgi::gState.currentY, x, y, bgi::gState.currentColor);
    bgi::gState.currentX = x;
    bgi::gState.currentY = y;
    flushIfVisible();
}

BGI_API void BGI_CALL moverel(int dx, int dy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.currentX += dx;
    bgi::gState.currentY += dy;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL moveto(int x, int y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.currentX = x;
    bgi::gState.currentY = y;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL outtext(char *textstring)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || textstring == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    const std::string text = safeText(textstring);
    drawTextAnchored(bgi::gState.currentX, bgi::gState.currentY, text);
    const auto [width, height] = bgi::measureText(text);
    if (bgi::gState.textSettings.direction == bgi::VERT_DIR)
    {
        bgi::gState.currentY += height;
    }
    else
    {
        bgi::gState.currentX += width;
    }
    flushIfVisible();
}

BGI_API void BGI_CALL outtextxy(int x, int y, char *textstring)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || textstring == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    drawTextAnchored(x, y, safeText(textstring));
    flushIfVisible();
}

BGI_API void BGI_CALL pieslice(int x, int y, int stangle, int endangle, int radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || radius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }
    fillSectorShape(x, y, stangle, endangle, radius, radius, true);
    flushIfVisible();
}

BGI_API void BGI_CALL putimage(int left, int top, void *bitmap, int op)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || !bgi::drawImage(left, top, bitmap, op))
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }
    flushIfVisible();
}

BGI_API void BGI_CALL putpixel(int x, int y, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }
    bgi::setPixelWithMode(x, y, color, bgi::gState.writeMode);
    flushIfVisible();
}

BGI_API void BGI_CALL rectangle(int left, int top, int right, int bottom)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    bgi::drawLineInternal(left, top, right, top, bgi::gState.currentColor);
    bgi::drawLineInternal(right, top, right, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(right, bottom, left, bottom, bgi::gState.currentColor);
    bgi::drawLineInternal(left, bottom, left, top, bgi::gState.currentColor);
    flushIfVisible();
}

BGI_API int BGI_CALL registerbgidriver(void (*driver)(void))
{
    (void)driver;
    return 0;
}

BGI_API int BGI_CALL registerbgifont(void (*font)(void))
{
    (void)font;
    return 0;
}

BGI_API void BGI_CALL restorecrtmode(void)
{
    closegraph();
}

BGI_API void BGI_CALL sector(int x, int y, int stangle, int endangle, int xradius, int yradius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady() || xradius < 0 || yradius < 0)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }
    fillSectorShape(x, y, stangle, endangle, xradius, yradius, true);
    flushIfVisible();
}

BGI_API void BGI_CALL setactivepage(int page)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.activePage = std::clamp(page, 0, bgi::kPageCount - 1);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setallpalette(const bgi::palettetype *palette)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (palette == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    bgi::gState.activePalette = *palette;
    for (int index = 0; index < bgi::kPaletteSize; ++index)
    {
        const int paletteIndex = bgi::normalizeColor(palette->colors[static_cast<std::size_t>(index)]);
        bgi::gState.palette[static_cast<std::size_t>(index)] = bgi::kBgiPalette[paletteIndex];
    }
    flushIfVisible();
}

BGI_API void BGI_CALL setaspectratio(int xasp, int yasp)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.aspectX = std::max(1, xasp);
    bgi::gState.aspectY = std::max(1, yasp);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setbkcolor(int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.bkColor = bgi::normalizeColor(color);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setcolor(int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.currentColor = bgi::normalizeColor(color);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setfillpattern(char *upattern, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (upattern == nullptr)
    {
        bgi::gState.lastResult = bgi::grInvalidInput;
        return;
    }

    std::memcpy(bgi::gState.fillMask.data(), upattern, bgi::kPatternRows);
    bgi::gState.fillPattern = bgi::USER_FILL;
    bgi::gState.userFillPatternEnabled = true;
    bgi::gState.fillColor = bgi::normalizeColor(color);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setfillstyle(int pattern, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.fillPattern = pattern;
    bgi::gState.fillColor = bgi::normalizeColor(color);
    bgi::gState.fillMask = bgi::makeFillPatternMask(pattern);
    bgi::gState.userFillPatternEnabled = pattern == bgi::USER_FILL;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API unsigned BGI_CALL setgraphbufsize(unsigned bufsize)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const unsigned previous = bgi::gState.graphBufSize;
    bgi::gState.graphBufSize = bufsize;
    return previous;
}

BGI_API void BGI_CALL setgraphmode(int mode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.graphMode = mode;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setlinestyle(int linestyle, unsigned upattern, int thickness)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.lineSettings.linestyle = linestyle;
    bgi::gState.lineSettings.upattern = upattern;
    bgi::gState.lineSettings.thickness = std::max(1, thickness);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setpalette(int colornum, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const int index = bgi::normalizeColor(colornum);
    const int mapped = bgi::normalizeColor(color);
    bgi::gState.activePalette.colors[static_cast<std::size_t>(index)] = mapped;
    bgi::gState.palette[static_cast<std::size_t>(index)] = bgi::kBgiPalette[mapped];
    flushIfVisible();
}

BGI_API void BGI_CALL setrgbpalette(int colornum, int red, int green, int blue)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const int index = bgi::normalizeColor(colornum);
    bgi::gState.palette[static_cast<std::size_t>(index)] = {
        bgi::normalizeColorByte(red),
        bgi::normalizeColorByte(green),
        bgi::normalizeColorByte(blue),
    };
    flushIfVisible();
}

BGI_API void BGI_CALL settextjustify(int horiz, int vert)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.textSettings.horiz = horiz;
    bgi::gState.textSettings.vert = vert;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL settextstyle(int font, int direction, int charsize)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.textSettings.font = font;
    bgi::gState.textSettings.direction = direction;
    bgi::gState.textSettings.charsize = std::max(1, charsize);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setusercharsize(int multx, int divx, int multy, int divy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.userCharSizeXNum = std::max(1, multx);
    bgi::gState.userCharSizeXDen = std::max(1, divx);
    bgi::gState.userCharSizeYNum = std::max(1, multy);
    bgi::gState.userCharSizeYDen = std::max(1, divy);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setviewport(int left, int top, int right, int bottom, int clip)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return;
    }

    const int minX = std::clamp(std::min(left, right), 0, bgi::gState.width - 1);
    const int maxX = std::clamp(std::max(left, right), 0, bgi::gState.width - 1);
    const int minY = std::clamp(std::min(top, bottom), 0, bgi::gState.height - 1);
    const int maxY = std::clamp(std::max(top, bottom), 0, bgi::gState.height - 1);
    bgi::gState.viewport = {minX, minY, maxX, maxY, clip};
    bgi::gState.currentX = 0;
    bgi::gState.currentY = 0;
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API void BGI_CALL setvisualpage(int page)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.visualPage = std::clamp(page, 0, bgi::kPageCount - 1);
    flushIfVisible();
}

BGI_API void BGI_CALL setwritemode(int mode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.writeMode = std::clamp(mode, bgi::COPY_PUT, bgi::NOT_PUT);
    bgi::gState.lastResult = bgi::grOk;
}

BGI_API int BGI_CALL swapbuffers(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!requireReady())
    {
        return -1;
    }
    std::swap(bgi::gState.activePage, bgi::gState.visualPage);
    flushIfVisible();
    return 0;
}

BGI_API int BGI_CALL textheight(char *textstring)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::measureText(safeText(textstring)).second;
}

BGI_API int BGI_CALL textwidth(char *textstring)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::measureText(safeText(textstring)).first;
}
