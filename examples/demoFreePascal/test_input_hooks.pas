program test_input_hooks;
{$IFDEF WXBGI_ENABLE_TEST_SEAMS}

uses
  ctypes, SysUtils;

function initwindow(width, height: cint; title: PChar; left, top, dbflag, closeflag: cint): pointer; cdecl; external 'wx_bgi_opengl';
procedure closegraph; cdecl; external 'wx_bgi_opengl';
function wxbgi_is_ready: cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_key_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_cursor_pos_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_mouse_button_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_char_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_key(key, scancode, action, mods: cint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_cursor(x, y: cdouble): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_char(codepoint: cuint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_test_simulate_mouse_button(btn, action, mods: cint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_is_key_down(key: cint): cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_key_pressed: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_read_key: cint; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_get_mouse_pos(x, y: pcint); cdecl; external 'wx_bgi_opengl';
function wxbgi_mouse_moved: cint; cdecl; external 'wx_bgi_opengl';

const
  WXBGI_KEY_PRESS = 1;
  WXBGI_KEY_RELEASE = 0;
  WXBGI_MOUSE_LEFT = 0;

var
  gKeyCalled, gLastKey: cint;
  gCursorCalled: cint;
  gLastCursorX, gLastCursorY: cint;
  gMouseBtnCalled: cint;
  gCharCalled: cint;

procedure MyKeyHook(key, scan, action, mods: cint); cdecl;
begin
  if action = WXBGI_KEY_PRESS then begin Inc(gKeyCalled); gLastKey := key; end;
end;

procedure MyCursorHook(x, y: cdouble); cdecl;
begin
  Inc(gCursorCalled);
  gLastCursorX := Round(x);
  gLastCursorY := Round(y);
end;

procedure MyMouseBtnHook(btn, action, mods: cint); cdecl;
begin
  if action = WXBGI_KEY_PRESS then Inc(gMouseBtnCalled);
end;

procedure MyCharHook(cp: cuint); cdecl;
begin
  Inc(gCharCalled);
end;

procedure Check(cond: boolean; const msg: string);
begin
  if not cond then begin WriteLn('FAIL: ', msg); Halt(1); end;
end;

begin
  initwindow(320, 240, 'test_input_hooks_pascal', 0, 0, 0, 0);
  Check(wxbgi_is_ready = 1, 'is_ready');
  wxbgi_set_key_hook(@MyKeyHook);
  wxbgi_set_cursor_pos_hook(@MyCursorHook);
  wxbgi_set_mouse_button_hook(@MyMouseBtnHook);
  wxbgi_set_char_hook(@MyCharHook);

  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_PRESS, 0);
  Check(gKeyCalled = 1, 'key hook called');
  Check(gLastKey = 65, 'last key');
  Check(wxbgi_is_key_down(65) = 1, 'key down');

  wxbgi_test_simulate_key(65, 65, WXBGI_KEY_RELEASE, 0);
  Check(wxbgi_is_key_down(65) = 0, 'key up');

  wxbgi_test_simulate_char(72);
  Check(gCharCalled = 1, 'char hook called');
  Check(wxbgi_key_pressed = 1, 'key queue has char');
  Check(wxbgi_read_key = 72, 'read char H');

  wxbgi_test_simulate_cursor(100.0, 200.0);
  Check(gCursorCalled = 1, 'cursor hook called');
  Check(gLastCursorX = 100, 'cursor x');
  Check(gLastCursorY = 200, 'cursor y');

  wxbgi_test_simulate_mouse_button(WXBGI_MOUSE_LEFT, WXBGI_KEY_PRESS, 0);
  Check(gMouseBtnCalled = 1, 'mouse btn hook called');

  closegraph;
  WriteLn('test_input_hooks_pascal: PASS');
end.

{$ELSE}
begin
  WriteLn('SKIP: test_input_hooks_pascal requires WXBGI_ENABLE_TEST_SEAMS');
end.
{$ENDIF}
