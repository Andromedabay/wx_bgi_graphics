program DemoBgiApiCoverage;

{$mode objfpc}{$H+}

uses
  SysUtils;

{$IFDEF MSWINDOWS}
const
  BgiLib = 'wx_bgi_opengl.dll';
{$ELSE}
  {$IFDEF DARWIN}
const
  BgiLib = 'libwx_bgi_opengl.dylib';
  {$ELSE}
const
  BgiLib = 'libwx_bgi_opengl.so';
  {$ENDIF}
{$ENDIF}

const
  DETECT = 0;
  BLUE = 1;
  GREEN = 2;
  LIGHTCYAN = 11;
  LIGHTRED = 12;
  LIGHTMAGENTA = 13;
  YELLOW = 14;
  WHITE = 15;
  SOLID_LINE = 0;
  DASHED_LINE = 3;
  THICK_WIDTH = 3;
  SOLID_FILL = 1;
  HATCH_FILL = 7;
  XHATCH_FILL = 8;
  COPY_PUT = 0;
  XOR_PUT = 1;
  OR_PUT = 2;
  DEFAULT_FONT = 0;
  TRIPLEX_FONT = 1;
  SANS_SERIF_FONT = 4;
  GOTHIC_FONT = 8;
  HORIZ_DIR = 0;
  VERT_DIR = 1;
  LEFT_TEXT = 0;
  CENTER_TEXT = 1;
  TOP_TEXT = 2;

type
  TArcCoords = record
    x, y, xstart, ystart, xend, yend: LongInt;
  end;

  PArcCoords = ^TArcCoords;

  TFillSettings = record
    pattern, color: LongInt;
  end;

  PFillSettings = ^TFillSettings;

  TLineSettings = record
    linestyle: LongInt;
    upattern: Cardinal;
    thickness: LongInt;
  end;

  PLineSettings = ^TLineSettings;

  TPaletteType = record
    size: Byte;
    colors: array[0..15] of LongInt;
  end;

  PPaletteType = ^TPaletteType;

  TTextSettings = record
    font, direction, charsize, horiz, vert: LongInt;
  end;

  PTextSettings = ^TTextSettings;

  TViewportType = record
    left, top, right, bottom, clip: LongInt;
  end;

  PViewportType = ^TViewportType;

procedure arc(x, y, stangle, endangle, radius: LongInt); cdecl; external BgiLib;
procedure bar(left, top, right, bottom: LongInt); cdecl; external BgiLib;
procedure bar3d(left, top, right, bottom, depth, topflag: LongInt); cdecl; external BgiLib;
procedure circle(x, y, radius: LongInt); cdecl; external BgiLib;
procedure cleardevice; cdecl; external BgiLib;
procedure clearviewport; cdecl; external BgiLib;
procedure closegraph; cdecl; external BgiLib;
procedure delay(millisec: LongInt); cdecl; external BgiLib;
procedure detectgraph(graphdriver, graphmode: PLongInt); cdecl; external BgiLib;
procedure drawpoly(numpoints: LongInt; polypoints: PLongInt); cdecl; external BgiLib;
procedure ellipse(x, y, stangle, endangle, xradius, yradius: LongInt); cdecl; external BgiLib;
procedure fillellipse(x, y, xradius, yradius: LongInt); cdecl; external BgiLib;
procedure fillpoly(numpoints: LongInt; polypoints: PLongInt); cdecl; external BgiLib;
procedure floodfill(x, y, border: LongInt); cdecl; external BgiLib;
function getactivepage: LongInt; cdecl; external BgiLib;
procedure getarccoords(arccoords: PArcCoords); cdecl; external BgiLib;
procedure getaspectratio(xasp, yasp: PLongInt); cdecl; external BgiLib;
function getbkcolor: LongInt; cdecl; external BgiLib;
function getcolor: LongInt; cdecl; external BgiLib;
function getdefaultpalette: PPaletteType; cdecl; external BgiLib;
function getdrivername: PChar; cdecl; external BgiLib;
procedure getfillpattern(pattern: PChar); cdecl; external BgiLib;
procedure getfillsettings(fillinfo: PFillSettings); cdecl; external BgiLib;
function getgraphmode: LongInt; cdecl; external BgiLib;
procedure getimage(left, top, right, bottom: LongInt; bitmap: Pointer); cdecl; external BgiLib;
procedure getlinesettings(lineinfo: PLineSettings); cdecl; external BgiLib;
function getmaxcolor: LongInt; cdecl; external BgiLib;
function getmaxheight: LongInt; cdecl; external BgiLib;
function getmaxmode: LongInt; cdecl; external BgiLib;
function getmaxwidth: LongInt; cdecl; external BgiLib;
function getmaxx: LongInt; cdecl; external BgiLib;
function getmaxy: LongInt; cdecl; external BgiLib;
function getmodename(mode_number: LongInt): PChar; cdecl; external BgiLib;
procedure getmoderange(graphdriver: LongInt; lomode, himode: PLongInt); cdecl; external BgiLib;
procedure getpalette(palette: PPaletteType); cdecl; external BgiLib;
function getpalettesize: LongInt; cdecl; external BgiLib;
function getpixel(x, y: LongInt): LongInt; cdecl; external BgiLib;
procedure gettextsettings(texttypeinfo: PTextSettings); cdecl; external BgiLib;
procedure getviewsettings(viewport: PViewportType); cdecl; external BgiLib;
function getvisualpage: LongInt; cdecl; external BgiLib;
function getwindowheight: LongInt; cdecl; external BgiLib;
function getwindowwidth: LongInt; cdecl; external BgiLib;
function getx: LongInt; cdecl; external BgiLib;
function gety: LongInt; cdecl; external BgiLib;
procedure graphdefaults; cdecl; external BgiLib;
function grapherrormsg(errorcode: LongInt): PChar; cdecl; external BgiLib;
function graphresult: LongInt; cdecl; external BgiLib;
function imagesize(left, top, right, bottom: LongInt): Cardinal; cdecl; external BgiLib;
procedure initgraph(graphdriver, graphmode: PLongInt; pathtodriver: PChar); cdecl; external BgiLib;
function initwindow(width, height: LongInt; title: PChar; left, top, dbflag, closeflag: LongInt): LongInt; cdecl; external BgiLib;
function installuserdriver(name: PChar; detect: Pointer): LongInt; cdecl; external BgiLib;
function installuserfont(name: PChar): LongInt; cdecl; external BgiLib;
procedure line(x1, y1, x2, y2: LongInt); cdecl; external BgiLib;
procedure linerel(dx, dy: LongInt); cdecl; external BgiLib;
procedure lineto(x, y: LongInt); cdecl; external BgiLib;
procedure moverel(dx, dy: LongInt); cdecl; external BgiLib;
procedure moveto(x, y: LongInt); cdecl; external BgiLib;
procedure outtext(textstring: PChar); cdecl; external BgiLib;
procedure outtextxy(x, y: LongInt; textstring: PChar); cdecl; external BgiLib;
procedure pieslice(x, y, stangle, endangle, radius: LongInt); cdecl; external BgiLib;
procedure putimage(left, top: LongInt; bitmap: Pointer; op: LongInt); cdecl; external BgiLib;
procedure putpixel(x, y, color: LongInt); cdecl; external BgiLib;
procedure rectangle(left, top, right, bottom: LongInt); cdecl; external BgiLib;
function registerbgidriver(driver: Pointer): LongInt; cdecl; external BgiLib;
function registerbgifont(font: Pointer): LongInt; cdecl; external BgiLib;
procedure restorecrtmode; cdecl; external BgiLib;
procedure sector(x, y, stangle, endangle, xradius, yradius: LongInt); cdecl; external BgiLib;
procedure setactivepage(page: LongInt); cdecl; external BgiLib;
procedure setallpalette(palette: PPaletteType); cdecl; external BgiLib;
procedure setaspectratio(xasp, yasp: LongInt); cdecl; external BgiLib;
procedure setbkcolor(color: LongInt); cdecl; external BgiLib;
procedure setcolor(color: LongInt); cdecl; external BgiLib;
procedure setfillpattern(upattern: PChar; color: LongInt); cdecl; external BgiLib;
procedure setfillstyle(pattern, color: LongInt); cdecl; external BgiLib;
function setgraphbufsize(bufsize: Cardinal): Cardinal; cdecl; external BgiLib;
procedure setgraphmode(mode: LongInt); cdecl; external BgiLib;
procedure setlinestyle(linestyle: LongInt; upattern: Cardinal; thickness: LongInt); cdecl; external BgiLib;
procedure setpalette(colornum, color: LongInt); cdecl; external BgiLib;
procedure setrgbpalette(colornum, red, green, blue: LongInt); cdecl; external BgiLib;
procedure settextjustify(horiz, vert: LongInt); cdecl; external BgiLib;
procedure settextstyle(font, direction, charsize: LongInt); cdecl; external BgiLib;
procedure setusercharsize(multx, divx, multy, divy: LongInt); cdecl; external BgiLib;
procedure setviewport(left, top, right, bottom, clip: LongInt); cdecl; external BgiLib;
procedure setvisualpage(page: LongInt); cdecl; external BgiLib;
procedure setwritemode(mode: LongInt); cdecl; external BgiLib;
function swapbuffers: LongInt; cdecl; external BgiLib;
function textheight(textstring: PChar): LongInt; cdecl; external BgiLib;
function textwidth(textstring: PChar): LongInt; cdecl; external BgiLib;

{ Advanced non-BGI extension API }
function wxbgi_is_ready: LongInt; cdecl; external BgiLib;
function wxbgi_poll_events: LongInt; cdecl; external BgiLib;
function wxbgi_should_close: LongInt; cdecl; external BgiLib;
procedure wxbgi_request_close; cdecl; external BgiLib;
function wxbgi_make_context_current: LongInt; cdecl; external BgiLib;
function wxbgi_swap_window_buffers: LongInt; cdecl; external BgiLib;
function wxbgi_set_vsync(enabled: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_get_window_size(width, height: PLongInt): LongInt; cdecl; external BgiLib;
function wxbgi_get_framebuffer_size(width, height: PLongInt): LongInt; cdecl; external BgiLib;
function wxbgi_set_window_title(title: PChar): LongInt; cdecl; external BgiLib;
function wxbgi_get_time_seconds: Double; cdecl; external BgiLib;
function wxbgi_get_proc_address(procName: PChar): Pointer; cdecl; external BgiLib;
function wxbgi_get_gl_string(which: LongInt): PChar; cdecl; external BgiLib;
function wxbgi_begin_advanced_frame(r, g, b, a: Single; clearColor, clearDepth: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_end_advanced_frame(swapBuf: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_read_pixels_rgba8(x, y, w, h: LongInt; outBuffer: PByte; outBufferSize: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_write_pixels_rgba8(x, y, w, h: LongInt; inBuffer: PByte; inBufferSize: LongInt): LongInt; cdecl; external BgiLib;

procedure Require(Condition: Boolean; const Msg: string);
begin
  if not Condition then
  begin
    WriteLn(Msg);
    Halt(1);
  end;
end;

var
  gd, gm, loMode, hiMode, xasp, yasp, InitGraphStatus: LongInt;
  WinW, WinH, FbW, FbH: LongInt;
  ReadBuf: array[0..3] of Byte;
  WriteBuf: array[0..15] of Byte;
  ElapsedTime: Double;
  ArcInfo: TArcCoords;
  FillInfo: TFillSettings;
  LineInfo: TLineSettings;
  TextInfo: TTextSettings;
  ViewInfo: TViewportType;
  Palette: TPaletteType;
  DefaultPalette: PPaletteType;
  Pattern: array[0..7] of Char;
  ReturnedPattern: array[0..7] of Char;
  Polygon: array[0..9] of LongInt;
  Msg1, Msg2, Msg3: PChar;
  ImgSize: Cardinal;
  ImageBuffer: array of Byte;
begin
  gd := -1;
  gm := -1;
  detectgraph(@gd, @gm);
  Require(setgraphbufsize(4096) = 0, 'setgraphbufsize mismatch');
  Require(installuserdriver('dummy', nil) = -1, 'installuserdriver mismatch');
  Require(installuserfont('dummy') = -1, 'installuserfont mismatch');
  Require(registerbgidriver(nil) = 0, 'registerbgidriver mismatch');
  Require(registerbgifont(nil) = 0, 'registerbgifont mismatch');

  { Some CI runners may fail initgraph due to desktop/OpenGL session constraints.
    Continue with initwindow coverage so the API surface is still validated. }
  initgraph(@gd, @gm, nil);
  InitGraphStatus := graphresult;
  if InitGraphStatus = 0 then
    closegraph;

  Require(
    initwindow(640, 480, 'Pascal BGI Coverage', 80, 80, 1, 1) = 0,
    'initwindow failed (initgraph status=' + IntToStr(InitGraphStatus) + ': ' + String(grapherrormsg(InitGraphStatus)) + ')'
  );
  setgraphmode(0);
  setbkcolor(BLUE);
  setcolor(YELLOW);
  setviewport(20, 20, 619, 459, 1);
  rectangle(0, 0, getmaxx, getmaxy);

  setlinestyle(DASHED_LINE, $F0F0, THICK_WIDTH);
  moveto(20, 20);
  line(20, 20, 200, 60);
  lineto(260, 120);
  linerel(40, 30);
  moverel(-20, 30);
  putpixel(getx, gety, LIGHTMAGENTA);
  Require(getpixel(getx, gety) >= 0, 'getpixel failed');

  setlinestyle(SOLID_LINE, $FFFF, 1);
  circle(120, 160, 40);
  arc(120, 160, 30, 240, 60);
  ellipse(260, 150, 0, 300, 60, 30);
  getarccoords(@ArcInfo);

  setfillstyle(HATCH_FILL, LIGHTRED);
  bar(320, 40, 400, 120);
  bar3d(420, 40, 520, 120, 16, 1);
  fillellipse(480, 180, 48, 28);
  pieslice(120, 300, 20, 160, 55);
  sector(260, 300, 200, 340, 60, 35);

  Polygon[0] := 330; Polygon[1] := 220; Polygon[2] := 380; Polygon[3] := 180;
  Polygon[4] := 440; Polygon[5] := 210; Polygon[6] := 430; Polygon[7] := 280;
  Polygon[8] := 340; Polygon[9] := 290;
  drawpoly(5, @Polygon[0]);
  fillpoly(5, @Polygon[0]);

  setfillstyle(SOLID_FILL, LIGHTCYAN);
  rectangle(40, 340, 140, 420);
  floodfill(80, 380, YELLOW);

  Pattern[0] := Char($81); Pattern[1] := Char($42); Pattern[2] := Char($24); Pattern[3] := Char($18);
  Pattern[4] := Char($18); Pattern[5] := Char($24); Pattern[6] := Char($42); Pattern[7] := Char($81);
  setfillpattern(@Pattern[0], GREEN);
  getfillpattern(@ReturnedPattern[0]);
  getfillsettings(@FillInfo);

  settextstyle(TRIPLEX_FONT, HORIZ_DIR, 2);
  settextjustify(LEFT_TEXT, TOP_TEXT);
  setusercharsize(2, 1, 2, 1);
  Msg1 := 'BGI COVERAGE PASCAL';
  outtextxy(20, 20, Msg1);
  settextstyle(SANS_SERIF_FONT, VERT_DIR, 1);
  settextjustify(CENTER_TEXT, CENTER_TEXT);
  outtextxy(560, 200, 'VERT');
  settextstyle(GOTHIC_FONT, HORIZ_DIR, 1);
  moveto(40, 250);
  Msg2 := 'OUTTEXT WIDTH HEIGHT';
  outtext(Msg2);
  Require(textwidth(Msg2) > 0, 'textwidth failed');
  Require(textheight(Msg2) > 0, 'textheight failed');

  ImgSize := imagesize(320, 40, 400, 120);
  SetLength(ImageBuffer, ImgSize);
  getimage(320, 40, 400, 120, @ImageBuffer[0]);
  putimage(430, 300, @ImageBuffer[0], XOR_PUT);

  getlinesettings(@LineInfo);
  gettextsettings(@TextInfo);
  getviewsettings(@ViewInfo);
  setaspectratio(4, 3);
  getaspectratio(@xasp, @yasp);
  getpalette(@Palette);
  DefaultPalette := getdefaultpalette;
  setpalette(1, 9);
  setrgbpalette(2, 32, 220, 90);
  setallpalette(DefaultPalette);

  Require(getbkcolor = BLUE, 'getbkcolor mismatch');
  Require(getcolor >= 0, 'getcolor mismatch');
  Require(getmaxcolor = 15, 'getmaxcolor mismatch');
  Require(getmaxmode = 0, 'getmaxmode mismatch');
  Require(getwindowwidth > 0, 'window width invalid');
  Require(getwindowheight > 0, 'window height invalid');
  Require(getmaxwidth >= getwindowwidth, 'getmaxwidth invalid');
  Require(getmaxheight >= getwindowheight, 'getmaxheight invalid');
  Require(getpalettesize = 16, 'getpalettesize invalid');
  Require(StrLen(getdrivername) > 0, 'getdrivername failed');
  Require(StrLen(getmodename(0)) > 0, 'getmodename failed');
  getmoderange(DETECT, @loMode, @hiMode);

  setactivepage(1);
  setvisualpage(0);
  setwritemode(OR_PUT);
  putpixel(10, 10, GREEN);
  setwritemode(COPY_PUT);
  Require(getactivepage = 1, 'getactivepage mismatch');
  Require(getvisualpage = 0, 'getvisualpage mismatch');
  Require(swapbuffers = 0, 'swapbuffers failed');

  clearviewport;
  setviewport(0, 0, getwindowwidth - 1, getwindowheight - 1, 1);
  cleardevice;
  graphdefaults;
  settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
  Msg3 := 'BGI API COVERAGE PASCAL OK';
  outtextxy(30, 30, Msg3);
  delay(30);
  grapherrormsg(graphresult);

  { Advanced extension API coverage }
  Require(wxbgi_is_ready = 1, 'wxbgi_is_ready failed');
  Require(wxbgi_poll_events = 0, 'wxbgi_poll_events failed');
  Require(wxbgi_make_context_current = 0, 'wxbgi_make_context_current failed');
  Require(wxbgi_set_vsync(1) = 0, 'wxbgi_set_vsync failed');
  Require(wxbgi_set_window_title('Pascal Advanced Coverage') = 0, 'wxbgi_set_window_title failed');

  WinW := 0; WinH := 0; FbW := 0; FbH := 0;
  Require(wxbgi_get_window_size(@WinW, @WinH) = 0, 'wxbgi_get_window_size failed');
  Require(wxbgi_get_framebuffer_size(@FbW, @FbH) = 0, 'wxbgi_get_framebuffer_size failed');
  Require(WinW > 0, 'window width invalid');
  Require(WinH > 0, 'window height invalid');
  Require(FbW > 0, 'framebuffer width invalid');
  Require(FbH > 0, 'framebuffer height invalid');

  ElapsedTime := wxbgi_get_time_seconds;
  Require(ElapsedTime >= 0.0, 'wxbgi_get_time_seconds failed');
  Require(wxbgi_get_proc_address('glClear') <> nil, 'wxbgi_get_proc_address failed');
  Require(wxbgi_get_gl_string(1) <> nil, 'wxbgi_get_gl_string failed');
  Require(StrLen(wxbgi_get_gl_string(2)) > 0, 'wxbgi_get_gl_string version empty');

  Require(wxbgi_begin_advanced_frame(0.05, 0.08, 0.12, 1.0, 1, 0) = 0, 'wxbgi_begin_advanced_frame failed');
  Require(wxbgi_end_advanced_frame(0) = 0, 'wxbgi_end_advanced_frame failed');

  WriteBuf[0] := 255; WriteBuf[1] := 32;  WriteBuf[2] := 32;  WriteBuf[3] := 255;
  WriteBuf[4] := 32;  WriteBuf[5] := 255; WriteBuf[6] := 32;  WriteBuf[7] := 255;
  WriteBuf[8] := 32;  WriteBuf[9] := 32;  WriteBuf[10] := 255; WriteBuf[11] := 255;
  WriteBuf[12] := 255; WriteBuf[13] := 255; WriteBuf[14] := 32; WriteBuf[15] := 255;
  Require(wxbgi_write_pixels_rgba8(1, 1, 2, 2, @WriteBuf[0], SizeOf(WriteBuf)) > 0, 'wxbgi_write_pixels_rgba8 failed');

  FillChar(ReadBuf, SizeOf(ReadBuf), 0);
  Require(wxbgi_read_pixels_rgba8(0, 0, 1, 1, @ReadBuf[0], SizeOf(ReadBuf)) > 0, 'wxbgi_read_pixels_rgba8 failed');
  Require(wxbgi_swap_window_buffers = 0, 'wxbgi_swap_window_buffers failed');
  Require(wxbgi_should_close = 0, 'wxbgi_should_close unexpected');

  restorecrtmode;
  WriteLn('Pascal coverage completed');
end.