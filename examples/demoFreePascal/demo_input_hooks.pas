program demo_input_hooks;

uses
  ctypes, SysUtils;

function initwindow(width, height: cint; title: PChar; left, top, dbflag, closeflag: cint): pointer; cdecl; external 'wx_bgi_opengl';
procedure closegraph; cdecl; external 'wx_bgi_opengl';
function wxbgi_should_close: cint; cdecl; external 'wx_bgi_opengl';
function wxbgi_poll_events: cint; cdecl; external 'wx_bgi_opengl';
procedure cleardevice; cdecl; external 'wx_bgi_opengl';
procedure setcolor(color: cint); cdecl; external 'wx_bgi_opengl';
procedure outtextxy(x, y: cint; text: PChar); cdecl; external 'wx_bgi_opengl';
procedure swapbuffers; cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_key_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';
procedure wxbgi_set_cursor_pos_hook(cb: pointer); cdecl; external 'wx_bgi_opengl';

const
  WHITE = 15;
  WXBGI_KEY_PRESS = 1;

var
  gKeyLog: string;
  gMouseX, gMouseY: cint;

procedure OnKey(key, scan, action, mods: cint); cdecl;
var s: string;
begin
  if action = WXBGI_KEY_PRESS then begin
    Str(key, s);
    gKeyLog := gKeyLog + 'K' + s + ' ';
    if Length(gKeyLog) > 60 then
      gKeyLog := Copy(gKeyLog, Length(gKeyLog) - 59, 60);
  end;
end;

procedure OnCursor(x, y: cdouble); cdecl;
begin
  gMouseX := Round(x);
  gMouseY := Round(y);
end;

var
  buf: array[0..79] of char;
  startMs: Int64;
  elapsed: Int64;

begin
  initwindow(640, 480, 'Input Hooks Demo (Pascal)', 100, 100, 1, 0);
  wxbgi_set_key_hook(@OnKey);
  wxbgi_set_cursor_pos_hook(@OnCursor);

  gKeyLog := '';
  gMouseX := 0; gMouseY := 0;
  startMs := Round(Now * 86400000);

  while wxbgi_should_close = 0 do
  begin
    wxbgi_poll_events;
    cleardevice;
    setcolor(WHITE);
    outtextxy(10, 10, 'Input Hooks Demo (Pascal) -- press keys or move mouse');
    outtextxy(10, 30, PChar('Keys: ' + gKeyLog));
    StrPCopy(buf, Format('Mouse: (%d, %d)', [gMouseX, gMouseY]));
    outtextxy(10, 50, buf);
    swapbuffers;

    elapsed := Round(Now * 86400000) - startMs;
    if elapsed >= 5000 then break;
  end;
  closegraph;
end.
