program DemoInputHooks;
(**
 * @file demo_input_hooks.pas
 * @brief Interactive Pascal demo: user input hook callbacks.
 *
 * Demonstrates all four hook types in real time:
 *  - Key hook:         status panel + arrow keys move a red marker; C cycles colour.
 *  - Char hook:        typed characters appear in a text bar at the bottom.
 *  - Cursor pos hook:  yellow crosshair follows the mouse.
 *  - Mouse button hook: left-click stamps a filled circle; right-click clears stamps.
 *
 * Press Escape or close the window to exit.
 *)

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
  BLACK         = 0;
  CYAN          = 3;
  GREEN         = 2;
  YELLOW        = 14;
  WHITE         = 15;
  LIGHTCYAN     = 11;
  LIGHTGREEN    = 10;
  LIGHTMAGENTA  = 13;
  LIGHTRED      = 12;
  LIGHTGRAY     = 7;
  SOLID_FILL    = 1;
  DEFAULT_FONT  = 0;
  HORIZ_DIR     = 0;

  WXBGI_KEY_PRESS   = 1;
  WXBGI_KEY_RELEASE = 0;
  WXBGI_MOUSE_LEFT  = 0;
  WXBGI_MOUSE_RIGHT = 1;

  GLFW_KEY_ESCAPE = 256;
  GLFW_KEY_LEFT   = 263;
  GLFW_KEY_RIGHT  = 262;
  GLFW_KEY_UP     = 265;
  GLFW_KEY_DOWN   = 264;
  GLFW_KEY_C      = 67;

  kW = 900;
  kH = 580;
  kPH = 52;
  kPW = (kW - 24) div 4;
  kPY = 4;
  kDrawY = kPY + kPH + 14;
  kTextY = kH - 34;
  MAX_STAMPS = 256;

type
  TStamp = record
    X, Y, Color: LongInt;
  end;

  TWxbgiKeyHook         = procedure(key, scancode, action, mods: LongInt); cdecl;
  TWxbgiCharHook        = procedure(codepoint: LongWord); cdecl;
  TWxbgiCursorPosHook   = procedure(x, y: LongInt); cdecl;
  TWxbgiMouseButtonHook = procedure(button, action, mods: LongInt); cdecl;

{ ----- BGI / ext API ----- }
function  initwindow(width, height: LongInt; title: PChar;
                     left, top, dbflag, closeflag: LongInt): LongInt;
          cdecl; external BgiLib;
procedure closegraph;         cdecl; external BgiLib;
procedure wxbgi_wx_app_create; cdecl; external BgiLib;
procedure wxbgi_wx_frame_create(width, height: LongInt; title: PChar); cdecl; external BgiLib;
procedure wxbgi_wx_app_main_loop; cdecl; external BgiLib;
procedure wxbgi_wx_close_frame; cdecl; external BgiLib;
procedure wxbgi_wx_set_idle_callback(fn: Pointer); cdecl; external BgiLib;
procedure wxbgi_wx_set_frame_rate(fps: LongInt); cdecl; external BgiLib;
procedure cleardevice; cdecl; external BgiLib;
function  swapbuffers: LongInt; cdecl; external BgiLib;
procedure setbkcolor(c: LongInt); cdecl; external BgiLib;
procedure setcolor(c: LongInt);   cdecl; external BgiLib;
procedure settextstyle(font, dir, sz: LongInt); cdecl; external BgiLib;
procedure outtextxy(x, y: LongInt; s: PChar); cdecl; external BgiLib;
procedure rectangle(x1, y1, x2, y2: LongInt); cdecl; external BgiLib;
procedure circle(x, y, r: LongInt); cdecl; external BgiLib;
procedure line(x1, y1, x2, y2: LongInt); cdecl; external BgiLib;
procedure setfillstyle(pat, col: LongInt); cdecl; external BgiLib;
procedure fillellipse(x, y, rx, ry: LongInt); cdecl; external BgiLib;
function  wxbgi_should_close: LongInt;    cdecl; external BgiLib;
procedure wxbgi_request_close;            cdecl; external BgiLib;
function  wxbgi_key_pressed: LongInt;     cdecl; external BgiLib;
function  wxbgi_read_key: LongInt;        cdecl; external BgiLib;
function  wxbgi_begin_advanced_frame(r, g, b, a: Single; cc, cd: LongInt): LongInt;
          cdecl; external BgiLib;
function  wxbgi_end_advanced_frame(swap: LongInt): LongInt;
          cdecl; external BgiLib;
procedure wxbgi_set_key_hook(cb: TWxbgiKeyHook);         cdecl; external BgiLib;
procedure wxbgi_set_char_hook(cb: TWxbgiCharHook);        cdecl; external BgiLib;
procedure wxbgi_set_cursor_pos_hook(cb: TWxbgiCursorPosHook); cdecl; external BgiLib;
procedure wxbgi_set_mouse_button_hook(cb: TWxbgiMouseButtonHook); cdecl; external BgiLib;

{ ----- Application state (written by hooks, read by render loop) ----- }
var
  { Status panel labels / last event strings }
  gKeyLast, gCharLast, gCursorLast, gMouseLast: ShortString;
  gKeyCount, gCharCount, gCursorCount, gMouseCount: LongInt;

  gCursorX, gCursorY: LongInt;   { mouse cursor position }
  gMarkerX, gMarkerY: LongInt;   { keyboard-moved marker }

  gStamps    : array[0..MAX_STAMPS - 1] of TStamp;
  gStampCount: LongInt;
  gStampColor: LongInt;

  gTyped: ShortString;

  gColorIdx: LongInt;

  { Colour cycle table }
  kColors: array[0..5] of LongInt;

  { Loop variable }

{ ----- Hook callbacks — do NOT call any wxbgi_* here (mutex is held) ----- }
procedure HookKey(key, scancode, action, mods: LongInt); cdecl;
const
  kStep = 8;
begin
  if action <> WXBGI_KEY_RELEASE then
    Inc(gKeyCount);

  if action = WXBGI_KEY_PRESS then
    gKeyLast := 'key=' + IntToStr(key) + ' PRESS'
  else if action = WXBGI_KEY_RELEASE then
    gKeyLast := 'key=' + IntToStr(key) + ' REL'
  else
    gKeyLast := 'key=' + IntToStr(key) + ' RPT';

  if action <> WXBGI_KEY_RELEASE then
  begin
    if key = GLFW_KEY_LEFT  then Dec(gMarkerX, kStep);
    if key = GLFW_KEY_RIGHT then Inc(gMarkerX, kStep);
    if key = GLFW_KEY_UP    then Dec(gMarkerY, kStep);
    if key = GLFW_KEY_DOWN  then Inc(gMarkerY, kStep);
    if (key = GLFW_KEY_C) and (action = WXBGI_KEY_PRESS) then
    begin
      gColorIdx  := (gColorIdx + 1) mod 6;
      gStampColor := kColors[gColorIdx];
    end;
  end;
end;

procedure HookChar(codepoint: LongWord); cdecl;
begin
  Inc(gCharCount);
  gCharLast := '''' + Chr(codepoint) + ''' (' + IntToStr(codepoint) + ')';
  if (codepoint = 8) and (Length(gTyped) > 0) then
    Delete(gTyped, Length(gTyped), 1)
  else if (codepoint >= 32) and (codepoint <= 126) and (Length(gTyped) < 40) then
    gTyped := gTyped + Chr(codepoint);
end;

procedure HookCursor(x, y: LongInt); cdecl;
begin
  Inc(gCursorCount);
  gCursorX := x;
  gCursorY := y;
  gCursorLast := '(' + IntToStr(x) + ',' + IntToStr(y) + ')';
end;

procedure HookMouse(button, action, mods: LongInt); cdecl;
begin
  Inc(gMouseCount);
  if button = WXBGI_MOUSE_LEFT then
    gMouseLast := 'L'
  else
    gMouseLast := 'R';
  if action = WXBGI_KEY_PRESS then
    gMouseLast := gMouseLast + ' PRESS m=' + IntToStr(mods)
  else
    gMouseLast := gMouseLast + ' REL';

  if (button = WXBGI_MOUSE_LEFT) and (action = WXBGI_KEY_PRESS) and
     (gStampCount < MAX_STAMPS) then
  begin
    gStamps[gStampCount].X     := gCursorX;
    gStamps[gStampCount].Y     := gCursorY;
    gStamps[gStampCount].Color := gStampColor;
    Inc(gStampCount);
  end;
  if (button = WXBGI_MOUSE_RIGHT) and (action = WXBGI_KEY_PRESS) then
    gStampCount := 0;
end;

{ ----- Render helpers ----- }
procedure DrawCross(x, y, r, col: LongInt);
begin
  setcolor(col);
  line(x - r, y,     x + r, y);
  line(x,     y - r, x,     y + r);
  circle(x, y, r);
end;

procedure DrawPanel(px, py, pw, ph, col: LongInt;
                    const lbl, last: ShortString; cnt: LongInt);
begin
  setcolor(col);
  rectangle(px, py, px + pw, py + ph);
  outtextxy(px + 4, py + 4,  PChar(AnsiString(lbl)));
  setcolor(WHITE);
  outtextxy(px + 4, py + 20, PChar(AnsiString(last)));
  setcolor(col);
  outtextxy(px + pw - 44, py + 4,
            PChar(AnsiString('n=' + IntToStr(cnt))));
end;

{ ----- Frame callback for wx idle ----- }
procedure DrawFrame; cdecl;
var
  k2, i: LongInt;
begin
  { Drain queue for Escape }
  while wxbgi_key_pressed <> 0 do
  begin
    k2 := wxbgi_read_key;
    if k2 = 27 then
      wxbgi_wx_close_frame;
  end;

  cleardevice;

  { Status panels }
  DrawPanel(4,            kPY, kPW, kPH, LIGHTCYAN,   'KEY  HOOK', gKeyLast,    gKeyCount);
  DrawPanel(8 + kPW,      kPY, kPW, kPH, LIGHTGREEN,  'CHAR HOOK', gCharLast,   gCharCount);
  DrawPanel(12 + kPW * 2, kPY, kPW, kPH, YELLOW,      'CURSOR   ', gCursorLast, gCursorCount);
  DrawPanel(16 + kPW * 3, kPY, kPW, kPH, LIGHTMAGENTA,'MOUSE    ', gMouseLast,  gMouseCount);

  { Instructions }
  setcolor(LIGHTGRAY);
  outtextxy(6, kDrawY,
    'Arrow keys = move marker | C = cycle colour | Left-click = stamp | Right-click = clear | type');

  { Stamps }
  for i := 0 to gStampCount - 1 do
  begin
    setfillstyle(SOLID_FILL, gStamps[i].Color);
    setcolor(gStamps[i].Color);
    fillellipse(gStamps[i].X, gStamps[i].Y, 14, 14);
  end;

  { Keyboard-controlled marker }
  DrawCross(gMarkerX, gMarkerY, 12, LIGHTRED);

  { Mouse cursor crosshair }
  DrawCross(gCursorX, gCursorY, 12, YELLOW);

  { Typed text bar }
  setcolor(CYAN);
  rectangle(4, kTextY - 4, kW - 4, kH - 6);
  setcolor(WHITE);
  outtextxy(10, kTextY, PChar(AnsiString('Typed: ' + gTyped)));

  swapbuffers;
end;

{ ----- Main ----- }
begin
  { Init colour cycle }
  kColors[0] := CYAN; kColors[1] := YELLOW;
  kColors[2] := LIGHTRED; kColors[3] := LIGHTGREEN;
  kColors[4] := LIGHTMAGENTA; kColors[5] := WHITE;

  gCursorX := kW div 2; gCursorY := kH div 2;
  gMarkerX := kW div 2; gMarkerY := kH div 2;
  gStampCount := 0; gStampColor := CYAN; gColorIdx := 0;
  gTyped := '';
  gKeyCount := 0; gCharCount := 0; gCursorCount := 0; gMouseCount := 0;
  gKeyLast := '(none)'; gCharLast := '(none)';
  gCursorLast := '(none)'; gMouseLast := '(none)';

  wxbgi_wx_app_create;
  wxbgi_wx_frame_create(kW, kH,
    'wx_BGI input hooks demo (Pascal) - Esc to exit');

  settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);

  wxbgi_set_key_hook(@HookKey);
  wxbgi_set_char_hook(@HookChar);
  wxbgi_set_cursor_pos_hook(@HookCursor);
  wxbgi_set_mouse_button_hook(@HookMouse);

  wxbgi_wx_set_idle_callback(@DrawFrame);
  wxbgi_wx_set_frame_rate(60);
  wxbgi_wx_app_main_loop;
end.
