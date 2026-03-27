#pragma once

#include "bgi_types.h"

/**
 * @file wx_bgi.h
 * @brief Public classic BGI-compatible C API exported by the library.
 */

/** @defgroup wxbgi_classic_api Classic BGI API
 *  @brief Borland-style graphics compatibility entry points.
 *  @{
 */

/** @brief Draws a circular arc. */
BGI_API void BGI_CALL arc(int x, int y, int stangle, int endangle, int radius);
/** @brief Draws a filled rectangle using the active fill style/color. */
BGI_API void BGI_CALL bar(int left, int top, int right, int bottom);
/** @brief Draws a pseudo-3D filled rectangle. */
BGI_API void BGI_CALL bar3d(int left, int top, int right, int bottom, int depth, int topflag);
/** @brief Draws a circle outline. */
BGI_API void BGI_CALL circle(int x, int y, int radius);
/** @brief Clears the full active drawing page to background color. */
BGI_API void BGI_CALL cleardevice(void);
/** @brief Clears only the active viewport region to background color. */
BGI_API void BGI_CALL clearviewport(void);
/** @brief Closes graphics window/context and releases associated resources. */
BGI_API void BGI_CALL closegraph(void);
/** @brief Sleeps for the requested duration in milliseconds. */
BGI_API void BGI_CALL delay(int millisec);
/** @brief Detects/initializes default graph driver and mode values. */
BGI_API void BGI_CALL detectgraph(int *graphdriver, int *graphmode);
/** @brief Draws a polygon outline from an array of x/y point pairs. */
BGI_API void BGI_CALL drawpoly(int numpoints, const int *polypoints);
/** @brief Draws an elliptical arc between start and end angles. */
BGI_API void BGI_CALL ellipse(int x, int y, int stangle, int endangle, int xradius, int yradius);
/** @brief Draws and fills an ellipse using current fill settings. */
BGI_API void BGI_CALL fillellipse(int x, int y, int xradius, int yradius);
/** @brief Fills a polygon interior from an array of x/y point pairs. */
BGI_API void BGI_CALL fillpoly(int numpoints, const int *polypoints);
/** @brief Flood-fills a region until the border color is reached. */
BGI_API void BGI_CALL floodfill(int x, int y, int border);
/** @brief Returns the currently selected active page index. */
BGI_API int BGI_CALL getactivepage(void);
/** @brief Retrieves cached arc endpoint coordinates from the last arc-like call. */
BGI_API void BGI_CALL getarccoords(bgi::arccoordstype *arccoords);
/** @brief Returns current aspect ratio numerators for x/y scaling. */
BGI_API void BGI_CALL getaspectratio(int *xasp, int *yasp);
/** @brief Returns the current background color index. */
BGI_API int BGI_CALL getbkcolor(void);
/** @brief Returns the current drawing color index. */
BGI_API int BGI_CALL getcolor(void);
/** @brief Returns a pointer to the default palette mapping. */
BGI_API bgi::palettetype *BGI_CALL getdefaultpalette(void);
/** @brief Returns the active graphics driver name string. */
BGI_API char *BGI_CALL getdrivername(void);
/** @brief Copies the current 8x8 fill pattern mask into @p pattern. */
BGI_API void BGI_CALL getfillpattern(char *pattern);
/** @brief Retrieves current fill pattern id and fill color. */
BGI_API void BGI_CALL getfillsettings(bgi::fillsettingstype *fillinfo);
/** @brief Returns the current graph mode id. */
BGI_API int BGI_CALL getgraphmode(void);
/** @brief Captures a rectangular image into a BGI image buffer. */
BGI_API void BGI_CALL getimage(int left, int top, int right, int bottom, void *bitmap);
/** @brief Retrieves current line style, user pattern, and thickness. */
BGI_API void BGI_CALL getlinesettings(bgi::linesettingstype *lineinfo);
/** @brief Returns the highest valid palette color index. */
BGI_API int BGI_CALL getmaxcolor(void);
/** @brief Returns the maximum window height supported by current mode. */
BGI_API int BGI_CALL getmaxheight(void);
/** @brief Returns the highest valid graphics mode identifier. */
BGI_API int BGI_CALL getmaxmode(void);
/** @brief Returns the maximum window width supported by current mode. */
BGI_API int BGI_CALL getmaxwidth(void);
/** @brief Returns the maximum x coordinate in the current viewport. */
BGI_API int BGI_CALL getmaxx(void);
/** @brief Returns the maximum y coordinate in the current viewport. */
BGI_API int BGI_CALL getmaxy(void);
/** @brief Returns a human-readable mode name for @p mode_number. */
BGI_API char *BGI_CALL getmodename(int mode_number);
/** @brief Returns the valid mode range for a given driver. */
BGI_API void BGI_CALL getmoderange(int graphdriver, int *lomode, int *himode);
/** @brief Retrieves the currently active palette mapping. */
BGI_API void BGI_CALL getpalette(bgi::palettetype *palette);
/** @brief Returns the number of entries in the active palette. */
BGI_API int BGI_CALL getpalettesize(void);
/** @brief Returns the color index of a pixel in viewport coordinates. */
BGI_API int BGI_CALL getpixel(int x, int y);
/** @brief Retrieves current text font, direction, size, and justification settings. */
BGI_API void BGI_CALL gettextsettings(bgi::textsettingstype *texttypeinfo);
/** @brief Returns the current viewport bounds and clip mode. */
BGI_API void BGI_CALL getviewsettings(bgi::viewporttype *viewport);
/** @brief Returns the currently selected visual page index. */
BGI_API int BGI_CALL getvisualpage(void);
/** @brief Returns current graphics window height. */
BGI_API int BGI_CALL getwindowheight(void);
/** @brief Returns current graphics window width. */
BGI_API int BGI_CALL getwindowwidth(void);
/** @brief Returns current pen x position. */
BGI_API int BGI_CALL getx(void);
/** @brief Returns current pen y position. */
BGI_API int BGI_CALL gety(void);
/** @brief Resets drawing state to BGI defaults while keeping the window open. */
BGI_API void BGI_CALL graphdefaults(void);
/** @brief Returns a text message for a graph subsystem error code. */
BGI_API char *BGI_CALL grapherrormsg(int errorcode);
/** @brief Returns the most recent graph subsystem status code. */
BGI_API int BGI_CALL graphresult(void);
/** @brief Returns the required byte size for a BGI image buffer region. */
BGI_API unsigned BGI_CALL imagesize(int left, int top, int right, int bottom);
/** @brief Initializes graphics using driver/mode pointers and optional driver path. */
BGI_API void BGI_CALL initgraph(int *graphdriver, int *graphmode, char *pathtodriver);
/** @brief Creates and initializes a graphics window and context. */
BGI_API int BGI_CALL initwindow(int width, int height, const char *title, int left, int top, int dbflag, int closeflag);
/** @brief Compatibility hook for installing a user graphics driver. */
BGI_API int BGI_CALL installuserdriver(char *name, void *detect);
/** @brief Compatibility hook for installing a user font. */
BGI_API int BGI_CALL installuserfont(char *name);
/** @brief Draws a line between two points. */
BGI_API void BGI_CALL line(int x1, int y1, int x2, int y2);
/** @brief Draws a line relative to current pen position and updates pen. */
BGI_API void BGI_CALL linerel(int dx, int dy);
/** @brief Draws a line to absolute coordinates from current pen position. */
BGI_API void BGI_CALL lineto(int x, int y);
/** @brief Moves pen position by relative delta without drawing. */
BGI_API void BGI_CALL moverel(int dx, int dy);
/** @brief Moves pen position to absolute coordinates without drawing. */
BGI_API void BGI_CALL moveto(int x, int y);
/** @brief Renders text at current pen position and advances the pen. */
BGI_API void BGI_CALL outtext(char *textstring);
/** @brief Renders text at explicit x/y coordinates. */
BGI_API void BGI_CALL outtextxy(int x, int y, char *textstring);
/** @brief Draws and fills a pie-slice sector. */
BGI_API void BGI_CALL pieslice(int x, int y, int stangle, int endangle, int radius);
/** @brief Draws an encoded BGI image buffer onto the active page. */
BGI_API void BGI_CALL putimage(int left, int top, void *bitmap, int op);
/** @brief Writes one pixel using current write mode semantics. */
BGI_API void BGI_CALL putpixel(int x, int y, int color);
/** @brief Draws a rectangle outline. */
BGI_API void BGI_CALL rectangle(int left, int top, int right, int bottom);
/** @brief Compatibility registration for a BGI driver callback. */
BGI_API int BGI_CALL registerbgidriver(void (*driver)(void));
/** @brief Compatibility registration for a BGI font callback. */
BGI_API int BGI_CALL registerbgifont(void (*font)(void));
/** @brief Restores text/console mode by closing graphics resources. */
BGI_API void BGI_CALL restorecrtmode(void);
/** @brief Draws and fills an elliptical sector. */
BGI_API void BGI_CALL sector(int x, int y, int stangle, int endangle, int xradius, int yradius);
/** @brief Selects which page receives drawing commands. */
BGI_API void BGI_CALL setactivepage(int page);
/** @brief Replaces entire palette mapping from a supplied palette structure. */
BGI_API void BGI_CALL setallpalette(const bgi::palettetype *palette);
/** @brief Sets aspect ratio numerators used by BGI geometry/text scaling. */
BGI_API void BGI_CALL setaspectratio(int xasp, int yasp);
/** @brief Sets background color index used by clear operations. */
BGI_API void BGI_CALL setbkcolor(int color);
/** @brief Sets active drawing color index. */
BGI_API void BGI_CALL setcolor(int color);
/** @brief Installs a custom 8x8 fill pattern and associated fill color. */
BGI_API void BGI_CALL setfillpattern(char *upattern, int color);
/** @brief Sets predefined fill pattern and fill color. */
BGI_API void BGI_CALL setfillstyle(int pattern, int color);
/** @brief Sets internal graph buffer size hint and returns previous value. */
BGI_API unsigned BGI_CALL setgraphbufsize(unsigned bufsize);
/** @brief Sets active graph mode identifier. */
BGI_API void BGI_CALL setgraphmode(int mode);
/** @brief Sets line style, user bit pattern, and thickness. */
BGI_API void BGI_CALL setlinestyle(int linestyle, unsigned upattern, int thickness);
/** @brief Maps a palette slot to a BGI color index. */
BGI_API void BGI_CALL setpalette(int colornum, int color);
/** @brief Sets true RGB value for a palette slot. */
BGI_API void BGI_CALL setrgbpalette(int colornum, int red, int green, int blue);
/** @brief Sets horizontal/vertical text justification mode. */
BGI_API void BGI_CALL settextjustify(int horiz, int vert);
/** @brief Sets text font family, direction, and character size. */
BGI_API void BGI_CALL settextstyle(int font, int direction, int charsize);
/** @brief Sets user character scaling fractions for x/y text axes. */
BGI_API void BGI_CALL setusercharsize(int multx, int divx, int multy, int divy);
/** @brief Sets viewport bounds and optional clipping behavior. */
BGI_API void BGI_CALL setviewport(int left, int top, int right, int bottom, int clip);
/** @brief Selects which page is displayed by flush/swap operations. */
BGI_API void BGI_CALL setvisualpage(int page);
/** @brief Sets pixel write mode (copy/xor/or/and/not). */
BGI_API void BGI_CALL setwritemode(int mode);
/** @brief Swaps active and visual pages for double-buffer style drawing. */
BGI_API int BGI_CALL swapbuffers(void);
/** @brief Returns text height in pixels for current text settings. */
BGI_API int BGI_CALL textheight(char *textstring);
/** @brief Returns text width in pixels for current text settings. */
BGI_API int BGI_CALL textwidth(char *textstring);

/** @} */