program test_input_bypass;
{$IFDEF WXBGI_ENABLE_TEST_SEAMS}

uses
  ctypes, SysUtils;

function initwindow(width, height: cint; title: PChar; left, top, dbflag, closeflag: cint): pointer; cdecl; external 'wx_bgi_opengl';
procedure closegraph; cdecl; external 'wx_bgi_opengl';
function wxbgi_is_ready: cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_scroll_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_key_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_cursor_pos_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_scroll(x, y: cdouble): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_key(key, scancode, action, mods: cint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_cursor(x, y: cdouble): cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_get_scroll_delta(dx, dy: pcdouble); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_input_defaults(flags: cint); cdecl; external 'wx_bgi_opengl';
function wxbgi_get_input_defaults: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_key_pressed: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_is_key_down(key: cint): cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_get_mouse_pos(x, y: pcint); cdecl; external 'wx_bgi_opengl';
function wxbgi_hk_get_mouse_x: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_hk_get_mouse_y: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_hk_dds_get_selected_count: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_hk_dds_get_selected_id(index: cint; buf: PChar; maxLen: cint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_hk_dds_is_selected(id: PChar): cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_hk_dds_select(id: PChar); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_hk_dds_deselect(id: PChar); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_hk_dds_deselect_all; cdecl; external 'wx_bgi_opengl';

const
  WXBGI_KEY_PRESS           = 1;
  WXBGI_KEY_RELEASE         = 0;
  WXBGI_DEFAULT_KEY_QUEUE   = $01;
  WXBGI_DEFAULT_CURSOR_TRACK = $02;
  WXBGI_DEFAULT_SCROLL_ACCUM = $08;
  WXBGI_DEFAULT_ALL         = $0F;

var
  gScrollCalled: cint;
  gScrollY: cdouble;
  gKeyHookCalled: cint;
  gCursorHookX: cdouble;

procedure ScrollHook(x, y: cdouble); cdecl;
begin
  gScrollY := y; Inc(gScrollCalled);
end;

procedure BypassKeyHook(key, scan, action, mods: cint); cdecl;
begin
  Inc(gKeyHookCalled);
end;

procedure BypassCursorHook(x, y: cdouble); cdecl;
begin
  gCursorHookX := x;
end;

procedure Check(cond: boolean; const msg: string);
begin
  if not cond then begin WriteLn('FAIL: ', msg); Halt(1); end;
end;

var
  dx, dy: cdouble;
  bx, by: cint;
  buf: array[0..63] of char;

begin
  initwindow(320, 240, 'test_input_bypass_pascal', 0, 0, 0, 0);
  Check(wxbgi_is_ready = 1, 'is_ready');

  wxbgi_set_scroll_hook(@ScrollHook);
  wxbgi_test_simulate_scroll(0.0, 1.0);
  Check(gScrollCalled = 1, 'scroll hook called');
  Check(gScrollY = 1.0, 'scroll y');

  dx := -1; dy := -1;
  wxbgi_get_scroll_delta(@dx, @dy);
  Check(dy = 1.0, 'delta accumulated');
  wxbgi_get_scroll_delta(@dx, @dy);
  Check(dy = 0.0, 'delta cleared');

  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and not WXBGI_DEFAULT_SCROLL_ACCUM);
  gScrollCalled := 0;
  wxbgi_test_simulate_scroll(0.0, 5.0);
  Check(gScrollCalled = 1, 'hook fires w/o accum');
  wxbgi_get_scroll_delta(@dx, @dy);
  Check(dy = 0.0, 'no accumulation');
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_set_scroll_hook(nil);

  wxbgi_set_key_hook(@BypassKeyHook);
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and not WXBGI_DEFAULT_KEY_QUEUE);
  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
  Check(gKeyHookCalled = 1, 'key hook fires');
  Check(wxbgi_key_pressed = 0, 'queue empty');
  Check(wxbgi_is_key_down(65) = 1, 'keyDown set');
  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_set_key_hook(nil);

  wxbgi_set_cursor_pos_hook(@BypassCursorHook);
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_test_simulate_cursor(50.0, 60.0);
  wxbgi_get_mouse_pos(@bx, @by);
  Check((bx = 50) and (by = 60), 'baseline pos');

  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and not WXBGI_DEFAULT_CURSOR_TRACK);
  gCursorHookX := -1.0;
  wxbgi_test_simulate_cursor(99.0, 88.0);
  Check(gCursorHookX = 99.0, 'cursor hook fires');
  wxbgi_get_mouse_pos(@bx, @by);
  Check((bx = 50) and (by = 60), 'position not updated');
  wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL);
  wxbgi_set_cursor_pos_hook(nil);

  Check(wxbgi_hk_dds_get_selected_count = 0, 'empty selection');
  wxbgi_hk_dds_select('obj1');
  wxbgi_hk_dds_select('obj2');
  Check(wxbgi_hk_dds_get_selected_count = 2, '2 selected');
  Check(wxbgi_hk_dds_is_selected('obj1') = 1, 'obj1 selected');
  Check(wxbgi_hk_dds_is_selected('obj3') = 0, 'obj3 not selected');

  wxbgi_hk_dds_get_selected_id(0, buf, 64);
  Check(buf = 'obj1', 'first id is obj1');

  wxbgi_hk_dds_deselect('obj1');
  Check(wxbgi_hk_dds_get_selected_count = 1, '1 selected after deselect');
  wxbgi_hk_dds_deselect_all;
  Check(wxbgi_hk_dds_get_selected_count = 0, 'all deselected');

  closegraph;
  WriteLn('test_input_bypass_pascal: PASS');
end.

{$ELSE}
begin
  WriteLn('SKIP: test_input_bypass_pascal requires WXBGI_ENABLE_TEST_SEAMS');
end.
{$ENDIF}
