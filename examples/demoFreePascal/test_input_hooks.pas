program TestInputHooks;
(**
 * @file test_input_hooks.pas
 * @brief Automated test: user input hook callbacks (Pascal).
 *
 * Mirrors the logic of test_input_hooks.cpp:
 *
 * Phase 1 (always): verifies hook registration and nil-deregistration.
 * Phase 2 ({$IFDEF WXBGI_ENABLE_TEST_SEAMS}): drives the callback->hook
 *   pipeline via wxbgi_test_simulate_* and checks parameters + state.
 * Phase 3 (always): draws summary primitives.
 *
 * Exit code: 0 = pass, 1 = fail.
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
  BLACK    = 0;
  CYAN     = 3;
  GREEN    = 2;
  YELLOW   = 14;
  WHITE    = 15;
  LIGHTRED = 12;

  WXBGI_KEY_PRESS   = 1;
  WXBGI_KEY_RELEASE = 0;
  WXBGI_KEY_REPEAT  = 2;
  WXBGI_MOUSE_LEFT  = 0;
  WXBGI_MOUSE_RIGHT = 1;
  WXBGI_MOD_CTRL    = $0002;

  { GLFW key constants used in simulation }
  GLFW_KEY_UP = 265;

{ ----- BGI API declarations ----- }
procedure wxbgi_wx_app_create; cdecl; external BgiLib;
procedure wxbgi_wx_frame_create(width, height: LongInt; title: PChar); cdecl; external BgiLib;
procedure wxbgi_wx_app_main_loop; cdecl; external BgiLib;
procedure wxbgi_wx_close_after_ms(ms: LongInt); cdecl; external BgiLib;
function  graphresult: LongInt;
          cdecl; external BgiLib;
procedure setbkcolor(color: LongInt);
          cdecl; external BgiLib;
procedure setcolor(color: LongInt);
          cdecl; external BgiLib;
procedure outtextxy(x, y: LongInt; textstring: PChar);
          cdecl; external BgiLib;
procedure circle(x, y, radius: LongInt);
          cdecl; external BgiLib;
procedure rectangle(x1, y1, x2, y2: LongInt);
          cdecl; external BgiLib;
procedure cleardevice;
          cdecl; external BgiLib;
function  setgraphmode(mode: LongInt): LongInt;
          cdecl; external BgiLib;
function  wxbgi_poll_events: LongInt;
          cdecl; external BgiLib;
function  wxbgi_key_pressed: LongInt;
          cdecl; external BgiLib;
function  wxbgi_read_key: LongInt;
          cdecl; external BgiLib;
function  wxbgi_is_key_down(key: LongInt): LongInt;
          cdecl; external BgiLib;
procedure wxbgi_get_mouse_pos(x, y: PLongInt);
          cdecl; external BgiLib;
function  wxbgi_mouse_moved: LongInt;
          cdecl; external BgiLib;

{ ----- Hook typedefs ----- }
type
  TWxbgiKeyHook         = procedure(key, scancode, action, mods: LongInt); cdecl;
  TWxbgiCharHook        = procedure(codepoint: LongWord); cdecl;
  TWxbgiCursorPosHook   = procedure(x, y: LongInt); cdecl;
  TWxbgiMouseButtonHook = procedure(button, action, mods: LongInt); cdecl;

{ ----- Hook registration ----- }
procedure wxbgi_set_key_hook(cb: TWxbgiKeyHook);
          cdecl; external BgiLib;
procedure wxbgi_set_char_hook(cb: TWxbgiCharHook);
          cdecl; external BgiLib;
procedure wxbgi_set_cursor_pos_hook(cb: TWxbgiCursorPosHook);
          cdecl; external BgiLib;
procedure wxbgi_set_mouse_button_hook(cb: TWxbgiMouseButtonHook);
          cdecl; external BgiLib;

{$IFDEF WXBGI_ENABLE_TEST_SEAMS}
{ ----- Simulation test seams (only exported when TEST_SEAMS enabled) ----- }
function wxbgi_test_simulate_key(key, scancode, action, mods: LongInt): LongInt;
         cdecl; external BgiLib;
function wxbgi_test_simulate_char(codepoint: LongWord): LongInt;
         cdecl; external BgiLib;
function wxbgi_test_simulate_cursor(x, y: LongInt): LongInt;
         cdecl; external BgiLib;
function wxbgi_test_simulate_mouse_button(button, action, mods: LongInt): LongInt;
         cdecl; external BgiLib;
{$ENDIF}

{ ----- Global hook records (written from callbacks; read from main) ----- }
var
  gKeyCount    : LongInt;
  gKeyLast     : LongInt;
  gKeyAction   : LongInt;
  gKeyMods     : LongInt;

  gCharCount   : LongInt;
  gCharLast    : LongWord;

  gCursorCount : LongInt;
  gCursorX     : LongInt;
  gCursorY     : LongInt;

  gMouseCount  : LongInt;
  gMouseButton : LongInt;
  gMouseAction : LongInt;
  gMouseMods   : LongInt;

  gPrevKeyCount  : LongInt;
  gPrevMouseCount: LongInt;
  gMX, gMY      : LongInt;
  gPrevCharCount : LongInt;

{ ----- Hook callbacks — do NOT call any wxbgi_* here (mutex is held) ----- }
procedure HookKey(key, scancode, action, mods: LongInt); cdecl;
begin
  Inc(gKeyCount);
  gKeyLast   := key;
  gKeyAction := action;
  gKeyMods   := mods;
end;

procedure HookChar(codepoint: LongWord); cdecl;
begin
  Inc(gCharCount);
  gCharLast := codepoint;
end;

procedure HookCursor(x, y: LongInt); cdecl;
begin
  Inc(gCursorCount);
  gCursorX := x;
  gCursorY := y;
end;

procedure HookMouse(button, action, mods: LongInt); cdecl;
begin
  Inc(gMouseCount);
  gMouseButton := button;
  gMouseAction := action;
  gMouseMods   := mods;
end;

{ ----- Test helpers ----- }
procedure Fail(const msg: string);
begin
  WriteLn(ErrOutput, 'FAIL [test_input_hooks_pascal]: ' + msg);
  Halt(1);
end;

procedure Require(cond: Boolean; const msg: string);
begin
  if not cond then Fail(msg);
end;

{ ----- Main ----- }
begin
  { Initialise all state variables }
  gKeyCount := 0;  gKeyLast := -1;  gKeyAction := -1;  gKeyMods := -1;
  gCharCount := 0; gCharLast := 0;
  gCursorCount := 0; gCursorX := -1; gCursorY := -1;
  gMouseCount := 0; gMouseButton := -1; gMouseAction := -1; gMouseMods := -1;
  gPrevKeyCount := 0; gPrevMouseCount := 0; gPrevCharCount := 0;
  gMX := 0; gMY := 0;

  wxbgi_wx_app_create;
  wxbgi_wx_frame_create(400, 260, 'test_input_hooks_pascal');
  Require(graphresult = 0, 'wxbgi_wx_frame_create failed');

  { ---- Phase 1: registration / deregistration ---- }
  wxbgi_set_key_hook(@HookKey);
  wxbgi_set_char_hook(@HookChar);
  wxbgi_set_cursor_pos_hook(@HookCursor);
  wxbgi_set_mouse_button_hook(@HookMouse);

  wxbgi_set_key_hook(nil);
  wxbgi_set_char_hook(nil);
  wxbgi_set_cursor_pos_hook(nil);
  wxbgi_set_mouse_button_hook(nil);

  wxbgi_set_key_hook(@HookKey);
  wxbgi_set_char_hook(@HookChar);
  wxbgi_set_cursor_pos_hook(@HookCursor);
  wxbgi_set_mouse_button_hook(@HookMouse);

{$IFDEF WXBGI_ENABLE_TEST_SEAMS}
  { ---- Phase 2: full pipeline simulation ---- }

  { Key hook — GLFW_KEY_A = 65 }
  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
  Require(gKeyCount  = 1,               'key hook: not called on press');
  Require(gKeyLast   = 65,              'key hook: key value wrong');
  Require(gKeyAction = WXBGI_KEY_PRESS, 'key hook: press action wrong');
  Require(wxbgi_is_key_down(65) = 1,   'key hook: keyDown not set after press');

  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_REPEAT, 0);
  Require(gKeyCount  = 2,                'key hook: not called on repeat');
  Require(gKeyAction = WXBGI_KEY_REPEAT, 'key hook: repeat action wrong');
  Require(wxbgi_is_key_down(65) = 1,    'key hook: keyDown cleared on repeat');

  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
  Require(gKeyCount  = 3,                  'key hook: not called on release');
  Require(gKeyAction = WXBGI_KEY_RELEASE,  'key hook: release action wrong');
  Require(wxbgi_is_key_down(65) = 0,       'key hook: keyDown not cleared after release');

  { Special key: Up arrow queues {0, 72} }
  wxbgi_test_simulate_key(GLFW_KEY_UP, 72, WXBGI_KEY_PRESS, 0);
  Require(gKeyCount = 4,                'key hook: not called for Up arrow');
  Require(wxbgi_key_pressed = 1,        'key hook: Up arrow prefix not queued');
  Require(wxbgi_read_key = 0,           'key hook: Up arrow prefix byte wrong');
  Require(wxbgi_read_key = 72,          'key hook: Up arrow scancode wrong');

  { Char hook }
  wxbgi_test_simulate_char(LongWord(Ord('H')));
  Require(gCharCount = 1,                       'char hook: not called');
  Require(gCharLast  = LongWord(Ord('H')),       'char hook: codepoint wrong');
  Require(wxbgi_key_pressed = 1,                'char hook: char not queued');
  Require(wxbgi_read_key = Ord('H'),            'char hook: queued char wrong');

  { ESC is filtered — hook must NOT fire }
  gPrevCharCount := gCharCount;
  wxbgi_test_simulate_char(27);
  Require(gCharCount = gPrevCharCount,  'char hook: ESC should not fire hook');
  Require(wxbgi_key_pressed = 0,        'char hook: ESC must not be queued');

  { Out-of-range codepoint is filtered — hook must NOT fire }
  wxbgi_test_simulate_char(300);
  Require(gCharCount = gPrevCharCount,  'char hook: out-of-range should not fire hook');

  { Cursor hook }
  wxbgi_test_simulate_cursor(150, 200);
  Require(gCursorCount = 1,  'cursor hook: not called');
  Require(gCursorX = 150,    'cursor hook: x wrong');
  Require(gCursorY = 200,    'cursor hook: y wrong');
  wxbgi_get_mouse_pos(@gMX, @gMY);
  Require(gMX = 150, 'cursor hook: gState.mouseX not updated');
  Require(gMY = 200, 'cursor hook: gState.mouseY not updated');
  Require(wxbgi_mouse_moved = 1, 'cursor hook: mouseMoved not set');

  wxbgi_test_simulate_cursor(100, 80);
  Require(gCursorCount = 2,           'cursor hook: not called on second move');
  Require(gCursorX = 100,             'cursor hook: second move X wrong');
  Require(gCursorY = 80,              'cursor hook: second move Y wrong');

  { Mouse button hook }
  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
  Require(gMouseCount  = 1,               'mouse btn hook: not called on left press');
  Require(gMouseButton = WXBGI_MOUSE_LEFT,  'mouse btn hook: button wrong');
  Require(gMouseAction = WXBGI_KEY_PRESS,   'mouse btn hook: action wrong');

  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_RIGHT, WXBGI_KEY_PRESS, WXBGI_MOD_CTRL);
  Require(gMouseCount  = 2,                'mouse btn hook: right press not fired');
  Require(gMouseButton = WXBGI_MOUSE_RIGHT,  'mouse btn hook: right button wrong');
  Require(gMouseMods   = WXBGI_MOD_CTRL,     'mouse btn hook: Ctrl mod wrong');

  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_RELEASE, 0);
  Require(gMouseCount  = 3,                'mouse btn hook: release not fired');
  Require(gMouseAction = WXBGI_KEY_RELEASE,  'mouse btn hook: release action wrong');

  { Deregistration: hook must stop firing after nil }
  gPrevKeyCount := gKeyCount;
  wxbgi_set_key_hook(nil);
  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
  Require(gKeyCount = gPrevKeyCount, 'key hook: still fires after nil deregistration');

  gPrevMouseCount := gMouseCount;
  wxbgi_set_mouse_button_hook(nil);
  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
  Require(gMouseCount = gPrevMouseCount,
          'mouse btn hook: still fires after nil deregistration');

{$ENDIF}

  { ---- Phase 3: draw summary ---- }
  setbkcolor(BLACK);
  setgraphmode(0);
  cleardevice;
  setcolor(WHITE);
  outtextxy(10, 10, 'test_input_hooks_pascal: PASS');

{$IFDEF WXBGI_ENABLE_TEST_SEAMS}
  setcolor(YELLOW);
  circle(gCursorX, gCursorY, 20);
  setcolor(CYAN);
  rectangle(10, 40, 10 + gKeyCount * 10, 55);
  setcolor(GREEN);
  rectangle(10, 60, 10 + gCursorCount * 20, 75);
  setcolor(LIGHTRED);
  rectangle(10, 80, 10 + gMouseCount * 20, 95);
{$ELSE}
  setcolor(YELLOW);
  outtextxy(10, 30, 'Rebuild with WXBGI_ENABLE_TEST_SEAMS for full test');
  circle(200, 130, 40);
{$ENDIF}

  wxbgi_wx_close_after_ms(500);
  wxbgi_wx_app_main_loop;
  WriteLn('PASS [test_input_hooks_pascal]');
end.
