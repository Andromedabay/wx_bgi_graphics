import ctypes
import os
import sys
from pathlib import Path


class ArcCoords(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("xstart", ctypes.c_int), ("ystart", ctypes.c_int), ("xend", ctypes.c_int), ("yend", ctypes.c_int)]


class FillSettings(ctypes.Structure):
    _fields_ = [("pattern", ctypes.c_int), ("color", ctypes.c_int)]


class LineSettings(ctypes.Structure):
    _fields_ = [("linestyle", ctypes.c_int), ("upattern", ctypes.c_uint), ("thickness", ctypes.c_int)]


class PaletteType(ctypes.Structure):
    _fields_ = [("size", ctypes.c_ubyte), ("colors", ctypes.c_int * 16)]


class TextSettings(ctypes.Structure):
    _fields_ = [("font", ctypes.c_int), ("direction", ctypes.c_int), ("charsize", ctypes.c_int), ("horiz", ctypes.c_int), ("vert", ctypes.c_int)]


class ViewportType(ctypes.Structure):
    _fields_ = [("left", ctypes.c_int), ("top", ctypes.c_int), ("right", ctypes.c_int), ("bottom", ctypes.c_int), ("clip", ctypes.c_int)]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def configure_prototypes(lib):
    callback_type = ctypes.CFUNCTYPE(None)
    lib.arc.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.bar.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.bar3d.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.circle.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.cleardevice.argtypes = []
    lib.clearviewport.argtypes = []
    lib.closegraph.argtypes = []
    lib.delay.argtypes = [ctypes.c_int]
    lib.detectgraph.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.drawpoly.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    lib.ellipse.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.fillellipse.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.fillpoly.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    lib.floodfill.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.getactivepage.restype = ctypes.c_int
    lib.getarccoords.argtypes = [ctypes.POINTER(ArcCoords)]
    lib.getaspectratio.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.getbkcolor.restype = ctypes.c_int
    lib.getcolor.restype = ctypes.c_int
    lib.getdefaultpalette.restype = ctypes.POINTER(PaletteType)
    lib.getdrivername.restype = ctypes.c_char_p
    lib.getfillpattern.argtypes = [ctypes.c_char_p]
    lib.getfillsettings.argtypes = [ctypes.POINTER(FillSettings)]
    lib.getgraphmode.restype = ctypes.c_int
    lib.getimage.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    lib.getlinesettings.argtypes = [ctypes.POINTER(LineSettings)]
    lib.getmaxcolor.restype = ctypes.c_int
    lib.getmaxheight.restype = ctypes.c_int
    lib.getmaxmode.restype = ctypes.c_int
    lib.getmaxwidth.restype = ctypes.c_int
    lib.getmaxx.restype = ctypes.c_int
    lib.getmaxy.restype = ctypes.c_int
    lib.getmodename.argtypes = [ctypes.c_int]
    lib.getmodename.restype = ctypes.c_char_p
    lib.getmoderange.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.getpalette.argtypes = [ctypes.POINTER(PaletteType)]
    lib.getpalettesize.restype = ctypes.c_int
    lib.getpixel.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.getpixel.restype = ctypes.c_int
    lib.gettextsettings.argtypes = [ctypes.POINTER(TextSettings)]
    lib.getviewsettings.argtypes = [ctypes.POINTER(ViewportType)]
    lib.getvisualpage.restype = ctypes.c_int
    lib.getwindowheight.restype = ctypes.c_int
    lib.getwindowwidth.restype = ctypes.c_int
    lib.getx.restype = ctypes.c_int
    lib.gety.restype = ctypes.c_int
    lib.graphdefaults.argtypes = []
    lib.grapherrormsg.argtypes = [ctypes.c_int]
    lib.grapherrormsg.restype = ctypes.c_char_p
    lib.graphresult.restype = ctypes.c_int
    lib.imagesize.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.imagesize.restype = ctypes.c_uint
    lib.initgraph.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.c_char_p]
    lib.initwindow.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.initwindow.restype = ctypes.c_int
    lib.installuserdriver.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
    lib.installuserdriver.restype = ctypes.c_int
    lib.installuserfont.argtypes = [ctypes.c_char_p]
    lib.installuserfont.restype = ctypes.c_int
    lib.line.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.linerel.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.lineto.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.moverel.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.moveto.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.outtext.argtypes = [ctypes.c_char_p]
    lib.outtextxy.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
    lib.pieslice.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.putimage.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    lib.putpixel.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.rectangle.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.registerbgidriver.argtypes = [callback_type]
    lib.registerbgidriver.restype = ctypes.c_int
    lib.registerbgifont.argtypes = [callback_type]
    lib.registerbgifont.restype = ctypes.c_int
    lib.restorecrtmode.argtypes = []
    lib.sector.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.setactivepage.argtypes = [ctypes.c_int]
    lib.setallpalette.argtypes = [ctypes.POINTER(PaletteType)]
    lib.setaspectratio.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.setbkcolor.argtypes = [ctypes.c_int]
    lib.setcolor.argtypes = [ctypes.c_int]
    lib.setfillpattern.argtypes = [ctypes.c_char_p, ctypes.c_int]
    lib.setfillstyle.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.setgraphbufsize.argtypes = [ctypes.c_uint]
    lib.setgraphbufsize.restype = ctypes.c_uint
    lib.setgraphmode.argtypes = [ctypes.c_int]
    lib.setlinestyle.argtypes = [ctypes.c_int, ctypes.c_uint, ctypes.c_int]
    lib.setpalette.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.setrgbpalette.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.settextjustify.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.settextstyle.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.setusercharsize.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.setviewport.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.setvisualpage.argtypes = [ctypes.c_int]
    lib.setwritemode.argtypes = [ctypes.c_int]
    lib.swapbuffers.restype = ctypes.c_int
    lib.textheight.argtypes = [ctypes.c_char_p]
    lib.textheight.restype = ctypes.c_int
    lib.textwidth.argtypes = [ctypes.c_char_p]
    lib.textwidth.restype = ctypes.c_int

    lib.wxbgi_is_ready.restype = ctypes.c_int
    lib.wxbgi_poll_events.restype = ctypes.c_int
    lib.wxbgi_should_close.restype = ctypes.c_int
    lib.wxbgi_request_close.argtypes = []
    lib.wxbgi_make_context_current.restype = ctypes.c_int
    lib.wxbgi_swap_window_buffers.restype = ctypes.c_int
    lib.wxbgi_set_vsync.argtypes = [ctypes.c_int]
    lib.wxbgi_set_vsync.restype = ctypes.c_int
    lib.wxbgi_get_window_size.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.wxbgi_get_window_size.restype = ctypes.c_int
    lib.wxbgi_get_framebuffer_size.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.wxbgi_get_framebuffer_size.restype = ctypes.c_int
    lib.wxbgi_set_window_title.argtypes = [ctypes.c_char_p]
    lib.wxbgi_set_window_title.restype = ctypes.c_int
    lib.wxbgi_get_time_seconds.restype = ctypes.c_double
    lib.wxbgi_get_proc_address.argtypes = [ctypes.c_char_p]
    lib.wxbgi_get_proc_address.restype = ctypes.c_void_p
    lib.wxbgi_get_gl_string.argtypes = [ctypes.c_int]
    lib.wxbgi_get_gl_string.restype = ctypes.c_char_p
    lib.wxbgi_begin_advanced_frame.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int]
    lib.wxbgi_begin_advanced_frame.restype = ctypes.c_int
    lib.wxbgi_end_advanced_frame.argtypes = [ctypes.c_int]
    lib.wxbgi_end_advanced_frame.restype = ctypes.c_int
    lib.wxbgi_read_pixels_rgba8.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int]
    lib.wxbgi_read_pixels_rgba8.restype = ctypes.c_int
    return callback_type


def main() -> int:
    library_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / ("wx_bgi_opengl.dll" if os.name == "nt" else "libwx_bgi_opengl.so")
    lib = ctypes.CDLL(str(library_path))
    callback_type = configure_prototypes(lib)

    def _dummy() -> None:
        return None

    dummy = callback_type(_dummy)

    driver = ctypes.c_int(-1)
    mode = ctypes.c_int(-1)
    lib.detectgraph(ctypes.byref(driver), ctypes.byref(mode))
    require(lib.setgraphbufsize(4096) == 0, "setgraphbufsize mismatch")
    require(lib.installuserdriver(b"dummy", None) == -1, "installuserdriver should be stubbed")
    require(lib.installuserfont(b"dummy") == -1, "installuserfont should be stubbed")
    require(lib.registerbgidriver(dummy) == 0, "registerbgidriver failed")
    require(lib.registerbgifont(dummy) == 0, "registerbgifont failed")

    # Some CI runners may fail initgraph due to desktop/OpenGL session constraints.
    # Continue with initwindow coverage so the API surface is still validated.
    lib.initgraph(ctypes.byref(driver), ctypes.byref(mode), None)
    initgraph_status = lib.graphresult()
    if initgraph_status == 0:
        lib.closegraph()

    initwindow_result = lib.initwindow(640, 480, b"Python BGI Coverage", 80, 80, 1, 1)
    require(
        initwindow_result == 0,
        f"initwindow failed (initgraph status={initgraph_status}: {lib.grapherrormsg(initgraph_status).decode()})",
    )

    require(lib.wxbgi_is_ready() == 1, "wxbgi_is_ready failed")
    require(lib.wxbgi_poll_events() == 0, "wxbgi_poll_events failed")
    require(lib.wxbgi_make_context_current() == 0, "wxbgi_make_context_current failed")
    require(lib.wxbgi_set_vsync(1) == 0, "wxbgi_set_vsync failed")
    require(lib.wxbgi_set_window_title(b"Python BGI Coverage Advanced") == 0, "wxbgi_set_window_title failed")

    win_w = ctypes.c_int(0)
    win_h = ctypes.c_int(0)
    fb_w = ctypes.c_int(0)
    fb_h = ctypes.c_int(0)
    require(lib.wxbgi_get_window_size(ctypes.byref(win_w), ctypes.byref(win_h)) == 0, "wxbgi_get_window_size failed")
    require(lib.wxbgi_get_framebuffer_size(ctypes.byref(fb_w), ctypes.byref(fb_h)) == 0, "wxbgi_get_framebuffer_size failed")
    require(win_w.value > 0 and win_h.value > 0, "window size invalid")
    require(fb_w.value > 0 and fb_h.value > 0, "framebuffer size invalid")
    require(lib.wxbgi_get_time_seconds() >= 0.0, "wxbgi_get_time_seconds failed")
    require(lib.wxbgi_get_proc_address(b"glClear") is not None, "wxbgi_get_proc_address failed")
    require(lib.wxbgi_get_gl_string(0x1F02) is not None, "wxbgi_get_gl_string version missing")

    require(lib.wxbgi_begin_advanced_frame(0.05, 0.08, 0.12, 1.0, 1, 0) == 0, "wxbgi_begin_advanced_frame failed")
    require(lib.wxbgi_end_advanced_frame(0) == 0, "wxbgi_end_advanced_frame failed")
    pixel = (ctypes.c_ubyte * 4)()
    require(lib.wxbgi_read_pixels_rgba8(0, 0, 1, 1, pixel, 4) > 0, "wxbgi_read_pixels_rgba8 failed")
    require(lib.wxbgi_swap_window_buffers() == 0, "wxbgi_swap_window_buffers failed")
    require(lib.wxbgi_should_close() == 0, "wxbgi_should_close unexpected")

    lib.setgraphmode(0)
    lib.setbkcolor(1)
    lib.setcolor(14)
    lib.setviewport(20, 20, 619, 459, 1)
    lib.rectangle(0, 0, lib.getmaxx(), lib.getmaxy())
    lib.setlinestyle(3, 0xF0F0, 3)
    lib.moveto(20, 20)
    lib.line(20, 20, 200, 60)
    lib.lineto(260, 120)
    lib.linerel(40, 30)
    lib.moverel(-20, 30)
    lib.putpixel(lib.getx(), lib.gety(), 13)
    require(lib.getpixel(lib.getx(), lib.gety()) >= 0, "getpixel failed")

    lib.setlinestyle(0, 0xFFFF, 1)
    lib.circle(120, 160, 40)
    lib.arc(120, 160, 30, 240, 60)
    lib.ellipse(260, 150, 0, 300, 60, 30)
    arc_info = ArcCoords()
    lib.getarccoords(ctypes.byref(arc_info))

    lib.setfillstyle(7, 12)
    lib.bar(320, 40, 400, 120)
    lib.bar3d(420, 40, 520, 120, 16, 1)
    lib.fillellipse(480, 180, 48, 28)
    lib.pieslice(120, 300, 20, 160, 55)
    lib.sector(260, 300, 200, 340, 60, 35)
    polygon = (ctypes.c_int * 10)(330, 220, 380, 180, 440, 210, 430, 280, 340, 290)
    lib.drawpoly(5, polygon)
    lib.fillpoly(5, polygon)

    lib.setfillstyle(1, 11)
    lib.rectangle(40, 340, 140, 420)
    lib.floodfill(80, 380, 14)

    pattern = ctypes.create_string_buffer(bytes([0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81]))
    lib.setfillpattern(pattern, 10)
    returned_pattern = ctypes.create_string_buffer(8)
    lib.getfillpattern(returned_pattern)
    fill_info = FillSettings()
    lib.getfillsettings(ctypes.byref(fill_info))

    lib.settextstyle(1, 0, 2)
    lib.settextjustify(0, 2)
    lib.setusercharsize(2, 1, 2, 1)
    lib.outtextxy(20, 20, b"BGI COVERAGE PYTHON")
    lib.settextstyle(4, 1, 1)
    lib.settextjustify(1, 1)
    lib.outtextxy(560, 200, b"VERT")
    lib.settextstyle(8, 0, 1)
    lib.moveto(40, 250)
    lib.outtext(b"OUTTEXT WIDTH HEIGHT")
    require(lib.textwidth(b"OUTTEXT WIDTH HEIGHT") > 0, "textwidth failed")
    require(lib.textheight(b"OUTTEXT WIDTH HEIGHT") > 0, "textheight failed")

    image_size = lib.imagesize(320, 40, 400, 120)
    image_buffer = (ctypes.c_ubyte * image_size)()
    lib.getimage(320, 40, 400, 120, image_buffer)
    lib.putimage(430, 300, image_buffer, 1)

    line_info = LineSettings()
    text_info = TextSettings()
    view_info = ViewportType()
    palette = PaletteType()
    xasp = ctypes.c_int(0)
    yasp = ctypes.c_int(0)
    lib.getlinesettings(ctypes.byref(line_info))
    lib.gettextsettings(ctypes.byref(text_info))
    lib.getviewsettings(ctypes.byref(view_info))
    lib.setaspectratio(4, 3)
    lib.getaspectratio(ctypes.byref(xasp), ctypes.byref(yasp))
    lib.getpalette(ctypes.byref(palette))
    default_palette = lib.getdefaultpalette()
    lib.setpalette(1, 9)
    lib.setrgbpalette(2, 32, 220, 90)
    lib.setallpalette(default_palette)

    require(lib.getbkcolor() == 1, "getbkcolor mismatch")
    require(lib.getcolor() >= 0, "getcolor mismatch")
    require(lib.getmaxcolor() == 15, "getmaxcolor mismatch")
    require(lib.getmaxmode() == 0, "getmaxmode mismatch")
    require(lib.getwindowwidth() > 0 and lib.getwindowheight() > 0, "window size invalid")
    require(lib.getmaxwidth() >= lib.getwindowwidth(), "max width invalid")
    require(lib.getmaxheight() >= lib.getwindowheight(), "max height invalid")
    require(lib.getpalettesize() == 16, "palette size invalid")
    require(lib.getdrivername(), "getdrivername failed")
    require(lib.getmodename(0), "getmodename failed")
    low_mode = ctypes.c_int(-1)
    high_mode = ctypes.c_int(-1)
    lib.getmoderange(0, ctypes.byref(low_mode), ctypes.byref(high_mode))

    lib.setactivepage(1)
    lib.setvisualpage(0)
    lib.setwritemode(2)
    lib.putpixel(10, 10, 10)
    lib.setwritemode(0)
    require(lib.getactivepage() == 1, "setactivepage failed")
    require(lib.getvisualpage() == 0, "setvisualpage failed")
    require(lib.swapbuffers() == 0, "swapbuffers failed")

    lib.clearviewport()
    lib.setviewport(0, 0, lib.getwindowwidth() - 1, lib.getwindowheight() - 1, 1)
    lib.cleardevice()
    lib.graphdefaults()
    lib.outtextxy(30, 30, b"BGI API COVERAGE PYTHON OK")
    lib.delay(30)
    _ = lib.grapherrormsg(lib.graphresult())
    lib.restorecrtmode()
    print("Python coverage completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())