program TestInputBypass;
(**
 * @file test_input_bypass.pas
 * @brief Automated test: scroll hook, default-bypass flags, wxbgi_hk_* DDS
 *        functions (Pascal mirror of test_input_bypass.cpp).
 *
 * Build with -dWXBGI_ENABLE_TEST_SEAMS for automated/headless mode.
 * Exit code: 0 = all pass, 1 = failures.
 *)

{$mode objfpc}{$H+}

uses
  SysUtils, Math;

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
  WHITE   = 15;
  GREEN   = 2;
  CYAN    = 3;
  YELLOW  = 14;

  WXBGI_KEY_PRESS    = 1;
  WXBGI_KEY_RELEASE  = 0;
  WXBGI_MOUSE_LEFT   = 0;

  WXBGI_DEFAULT_KEY_QUEUE    = $01;
  WXBGI_DEFAULT_CURSOR_TRACK = $02;
  WXBGI_DEFAULT_MOUSE_PICK   = $04;
  WXBGI_DEFAULT_SCROLL_ACCUM = $08;
  WXBGI_DEFAULT_ALL          = $0F;
  WXBGI_DEFAULT_NONE         = $00;

  { GLFW_KEY_ESCAPE = 256 }
  GLFW_KEY_ESCAPE = 256;

// ---------------------------------------------------------------------------
// BGI API declarations
// ---------------------------------------------------------------------------
procedure wxbgi_wx_app_create; cdecl; external BgiLib name 'wxbgi_wx_app_create';
procedure wxbgi_wx_frame_create(width, height: Integer; title: PChar); cdecl; external BgiLib name 'wxbgi_wx_frame_create';
procedure wxbgi_wx_app_main_loop; cdecl; external BgiLib name 'wxbgi_wx_app_main_loop';
procedure wxbgi_wx_close_after_ms(ms: Integer); cdecl; external BgiLib name 'wxbgi_wx_close_after_ms';
function graphresult: Integer; cdecl;
  external BgiLib name 'graphresult';
function wxbgi_key_pressed: Integer; cdecl;
  external BgiLib name 'wxbgi_key_pressed';
procedure wxbgi_get_mouse_pos(x, y: PInteger); cdecl;
  external BgiLib name 'wxbgi_get_mouse_pos';

// Scroll API
procedure wxbgi_get_scroll_delta(dx, dy: PDouble); cdecl;
  external BgiLib name 'wxbgi_get_scroll_delta';

// Input defaults API
procedure wxbgi_set_input_defaults(flags: Integer); cdecl;
  external BgiLib name 'wxbgi_set_input_defaults';
function wxbgi_get_input_defaults: Integer; cdecl;
  external BgiLib name 'wxbgi_get_input_defaults';

// Hook registration
type
  TScrollHook = procedure(xoffset, yoffset: Double); cdecl;
  TKeyHook    = procedure(key, scan, action, mods: Integer); cdecl;
  TMouseHook  = procedure(button, action, mods: Integer); cdecl;

procedure wxbgi_set_scroll_hook(cb: TScrollHook); cdecl;
  external BgiLib name 'wxbgi_set_scroll_hook';
procedure wxbgi_set_key_hook(cb: TKeyHook); cdecl;
  external BgiLib name 'wxbgi_set_key_hook';
procedure wxbgi_set_mouse_button_hook(cb: TMouseHook); cdecl;
  external BgiLib name 'wxbgi_set_mouse_button_hook';

// Hook-context DDS functions
function wxbgi_hk_get_mouse_x: Integer; cdecl;
  external BgiLib name 'wxbgi_hk_get_mouse_x';
function wxbgi_hk_get_mouse_y: Integer; cdecl;
  external BgiLib name 'wxbgi_hk_get_mouse_y';
function wxbgi_hk_dds_get_selected_count: Integer; cdecl;
  external BgiLib name 'wxbgi_hk_dds_get_selected_count';
function wxbgi_hk_dds_get_selected_id(index: Integer; outId: PChar; maxLen: Integer): Integer; cdecl;
  external BgiLib name 'wxbgi_hk_dds_get_selected_id';
function wxbgi_hk_dds_is_selected(id: PChar): Integer; cdecl;
  external BgiLib name 'wxbgi_hk_dds_is_selected';
procedure wxbgi_hk_dds_select(id: PChar); cdecl;
  external BgiLib name 'wxbgi_hk_dds_select';
procedure wxbgi_hk_dds_deselect(id: PChar); cdecl;
  external BgiLib name 'wxbgi_hk_dds_deselect';
procedure wxbgi_hk_dds_deselect_all; cdecl;
  external BgiLib name 'wxbgi_hk_dds_deselect_all';
function wxbgi_hk_dds_pick_at(x, y, ctrl: Integer): Integer; cdecl;
  external BgiLib name 'wxbgi_hk_dds_pick_at';

// Selection public API
function wxbgi_selection_count: Integer; cdecl;
  external BgiLib name 'wxbgi_selection_count';
procedure wxbgi_selection_clear; cdecl;
  external BgiLib name 'wxbgi_selection_clear';

{$IFDEF WXBGI_ENABLE_TEST_SEAMS}
// Test seams
function wxbgi_test_clear_key_queue: Integer; cdecl;
  external BgiLib name 'wxbgi_test_clear_key_queue';
function wxbgi_test_simulate_key(key, scan, action, mods: Integer): Integer; cdecl;
  external BgiLib name 'wxbgi_test_simulate_key';
function wxbgi_test_simulate_cursor(x, y: Integer): Integer; cdecl;
  external BgiLib name 'wxbgi_test_simulate_cursor';
function wxbgi_test_simulate_mouse_button(button, action, mods: Integer): Integer; cdecl;
  external BgiLib name 'wxbgi_test_simulate_mouse_button';
function wxbgi_test_simulate_scroll(xoffset, yoffset: Double): Integer; cdecl;
  external BgiLib name 'wxbgi_test_simulate_scroll';
{$ENDIF}

// ---------------------------------------------------------------------------
// Shared hook state (global variables — FPC does not allow inline var)
// ---------------------------------------------------------------------------
var
  gScrollCalls  : Integer;
  gScrollLastX  : Double;
  gScrollLastY  : Double;
  gMouseCalls   : Integer;
  gHkMouseX     : Integer;
  gHkMouseY     : Integer;
  gHkSelCount   : Integer;
  gFailures     : Integer;

// ---------------------------------------------------------------------------
// Hook callbacks (plain cdecl procedures)
// ---------------------------------------------------------------------------
procedure CbScroll(x, y: Double); cdecl;
begin
  Inc(gScrollCalls);
  gScrollLastX := x;
  gScrollLastY := y;
end;

procedure CbScrollHkQuery(x, y: Double); cdecl;
begin
  gHkSelCount := wxbgi_hk_dds_get_selected_count;
  if wxbgi_hk_dds_is_selected('hk_obj_a') <> 0 then
    wxbgi_hk_dds_deselect('hk_obj_a')
  else
    wxbgi_hk_dds_select('hk_obj_a');
end;

procedure CbScrollHkDeselAll(x, y: Double); cdecl;
begin
  wxbgi_hk_dds_deselect_all;
end;

procedure CbMouse(button, action, mods: Integer); cdecl;
begin
  Inc(gMouseCalls);
  gHkMouseX := wxbgi_hk_get_mouse_x;
  gHkMouseY := wxbgi_hk_get_mouse_y;
end;

procedure CbKey(key, scan, action, mods: Integer); cdecl;
begin
  { no-op; just counts implicitly via gFailures test }
end;

// ---------------------------------------------------------------------------
// Assertion helper
// ---------------------------------------------------------------------------
procedure Check(cond: Boolean; const msg: string);
begin
  if cond then
    WriteLn('  PASS: ', msg)
  else
  begin
    WriteLn(ErrOutput, '  FAIL: ', msg);
    Inc(gFailures);
  end;
end;

// ---------------------------------------------------------------------------
{$IFDEF WXBGI_ENABLE_TEST_SEAMS}

procedure TestScrollHook;
var
  dx, dy: Double;
begin
  WriteLn;
  WriteLn('--- Phase 1: Scroll hook + accumulation ---');

  wxbgi_set_scroll_hook(@CbScroll);
  gScrollCalls := 0;
  wxbgi_test_simulate_scroll(1.0, 3.0);
  wxbgi_test_simulate_scroll(-0.5, 2.0);

  Check(gScrollCalls = 2,          'hook called twice');
  Check(gScrollLastX = -0.5,       'last xoffset = -0.5');
  Check(gScrollLastY = 2.0,        'last yoffset = 2.0');

  dx := 99.0; dy := 99.0;
  wxbgi_get_scroll_delta(@dx, @dy);
  Check(dx = 0.5, 'accumulated dx = 0.5');
  Check(dy = 5.0, 'accumulated dy = 5.0');

  wxbgi_get_scroll_delta(@dx, @dy);
  Check((dx = 0.0) and (dy = 0.0), 'delta cleared after read');

  wxbgi_set_scroll_hook(nil);
  gScrollCalls := 0;
  wxbgi_test_simulate_scroll(7.0, 4.0);
  Check(gScrollCalls = 0, 'no hook call after removal');
  wxbgi_get_scroll_delta(@dx, @dy);
  Check((dx = 7.0) and (dy = 4.0), 'delta accumulates without hook');
end;

procedure TestBypassFlags;
var
  mx, my: Integer;
  dx, dy: Double;
begin
  WriteLn;
  WriteLn('--- Phase 2: Default bypass flags ---');

  Check(wxbgi_get_input_defaults = WXBGI_DEFAULT_ALL, 'initial flags = WXBGI_DEFAULT_ALL');

  { KEY_QUEUE bypass }
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and (not WXBGI_DEFAULT_KEY_QUEUE));
  wxbgi_test_clear_key_queue;
  wxbgi_test_simulate_key(GLFW_KEY_ESCAPE, 1, WXBGI_KEY_PRESS, 0);
  Check(wxbgi_key_pressed = 0, 'queue empty when KEY_QUEUE bypassed');

  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_test_clear_key_queue;
  wxbgi_test_simulate_key(GLFW_KEY_ESCAPE, 1, WXBGI_KEY_PRESS, 0);
  Check(wxbgi_key_pressed <> 0, 'ESC queued when KEY_QUEUE enabled');
  wxbgi_test_clear_key_queue;

  { CURSOR_TRACK bypass }
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_test_simulate_cursor(100, 200);
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and (not WXBGI_DEFAULT_CURSOR_TRACK));
  wxbgi_test_simulate_cursor(999, 888);
  mx := 0; my := 0;
  wxbgi_get_mouse_pos(@mx, @my);
  Check((mx = 100) and (my = 200), 'mouse pos unchanged when CURSOR_TRACK bypassed');

  { SCROLL_ACCUM bypass }
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and (not WXBGI_DEFAULT_SCROLL_ACCUM));
  dx := 0.0; dy := 0.0;
  wxbgi_get_scroll_delta(@dx, @dy);
  wxbgi_test_simulate_scroll(5.0, 3.0);
  wxbgi_get_scroll_delta(@dx, @dy);
  Check((dx = 0.0) and (dy = 0.0), 'delta not accumulated when SCROLL_ACCUM bypassed');

  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  Check(wxbgi_get_input_defaults = WXBGI_DEFAULT_ALL, 'defaults restored');
end;

procedure TestHookContextDds;
var
  idbuf: array[0..255] of Char;
  r: Integer;
begin
  WriteLn;
  WriteLn('--- Phase 3: Hook-context DDS functions ---');

  { hk_get_mouse_x/y }
  wxbgi_test_simulate_cursor(42, 77);
  wxbgi_set_mouse_button_hook(@CbMouse);
  gMouseCalls := 0; gHkMouseX := -1; gHkMouseY := -1;
  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
  Check(gMouseCalls = 1,  'mouse hook called');
  Check(gHkMouseX = 42,  'hk_get_mouse_x = 42');
  Check(gHkMouseY = 77,  'hk_get_mouse_y = 77');
  wxbgi_set_mouse_button_hook(nil);

  { hk_dds_select / deselect / is_selected / count }
  wxbgi_selection_clear;
  wxbgi_hk_dds_select('test_obj_1');
  wxbgi_hk_dds_select('test_obj_2');
  Check(wxbgi_selection_count = 2,                  'selected 2 via hk_dds_select');
  Check(wxbgi_hk_dds_is_selected('test_obj_1') = 1, 'test_obj_1 is selected');
  Check(wxbgi_hk_dds_is_selected('test_obj_2') = 1, 'test_obj_2 is selected');
  Check(wxbgi_hk_dds_is_selected('missing') = 0,    'missing obj not selected');

  wxbgi_hk_dds_deselect('test_obj_1');
  Check(wxbgi_selection_count = 1,                  'count = 1 after deselect');
  Check(wxbgi_hk_dds_is_selected('test_obj_1') = 0, 'test_obj_1 deselected');

  Check(wxbgi_hk_dds_get_selected_count = 1, 'hk_get_selected_count = 1');
  r := wxbgi_hk_dds_get_selected_id(0, @idbuf[0], SizeOf(idbuf));
  Check((r > 0) and (StrPas(@idbuf[0]) = 'test_obj_2'), 'hk_get_selected_id(0) = test_obj_2');
  Check(wxbgi_hk_dds_get_selected_id(5, @idbuf[0], SizeOf(idbuf)) = -1,
        'hk_get_selected_id out-of-range = -1');

  { Toggle via scroll hook }
  gHkSelCount := -1;
  wxbgi_set_scroll_hook(@CbScrollHkQuery);
  wxbgi_test_simulate_scroll(0.0, 1.0);
  Check(gHkSelCount = 1, 'selection count = 1 in scroll hook');
  Check(wxbgi_hk_dds_is_selected('hk_obj_a') = 1, 'hk_obj_a added inside scroll hook');

  wxbgi_test_simulate_scroll(0.0, 1.0);
  Check(wxbgi_hk_dds_is_selected('hk_obj_a') = 0, 'hk_obj_a removed inside scroll hook');

  { deselect_all from hook }
  wxbgi_set_scroll_hook(@CbScrollHkDeselAll);
  wxbgi_hk_dds_select('x1');
  wxbgi_hk_dds_select('x2');
  wxbgi_test_simulate_scroll(0.0, 1.0);
  Check(wxbgi_selection_count = 0, 'deselect_all from hook clears selection');

  wxbgi_set_scroll_hook(nil);
  wxbgi_selection_clear;
end;

{$ENDIF WXBGI_ENABLE_TEST_SEAMS}

// ---------------------------------------------------------------------------
var
  automated: Boolean;
  i: Integer;
begin
  gScrollCalls := 0;
  gScrollLastX := 0.0;
  gScrollLastY := 0.0;
  gMouseCalls  := 0;
  gHkMouseX    := -1;
  gHkMouseY    := -1;
  gHkSelCount  := -1;
  gFailures    := 0;

  { Mask FPU exceptions before GTK/wx init — required on Linux/macOS.
    GTK's librsvg performs NaN/denormal operations that raise SIGFPE under
    FreePascal's default strict x87 exception mask. }
  SetExceptionMask([exInvalidOp, exDenormalized, exZeroDivide,
                    exOverflow, exUnderflow, exPrecision]);

  automated := False;
  for i := 1 to ParamCount do
    if (ParamStr(i) = '--simulate') or (ParamStr(i) = '-s') then
      automated := True;

{$IFDEF WXBGI_ENABLE_TEST_SEAMS}
  automated := True;
{$ENDIF}

  if not automated then
  begin
    WriteLn('Run with --simulate for headless automated tests.');
    Halt(0);
  end;

{$IFNDEF WXBGI_ENABLE_TEST_SEAMS}
  WriteLn('WXBGI_ENABLE_TEST_SEAMS not defined — tests unavailable.');
  Halt(0);
{$ELSE}
  WriteLn('=== test_input_bypass (Pascal): automated mode ===');

  wxbgi_wx_app_create;
  wxbgi_wx_frame_create(320, 240, 'test_input_bypass_pas');
  if graphresult <> 0 then
  begin
    WriteLn(ErrOutput, 'initwindow failed');
    Halt(1);
  end;

  TestScrollHook;
  TestBypassFlags;
  TestHookContextDds;

  wxbgi_wx_close_after_ms(500);
  wxbgi_wx_app_main_loop;

  if gFailures = 0 then
    WriteLn('All test_input_bypass Pascal tests PASSED.')
  else
  begin
    WriteLn(ErrOutput, IntToStr(gFailures) + ' test(s) FAILED.');
    Halt(1);
  end;
{$ENDIF}
end.
