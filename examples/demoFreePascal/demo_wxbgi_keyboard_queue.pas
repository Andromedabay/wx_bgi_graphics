program DemoWxbgiKeyboardQueue;

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
  BLACK = 0;
  WHITE = 15;
  YELLOW = 14;
  LIGHTGREEN = 10;

  DEFAULT_FONT = 0;
  HORIZ_DIR = 0;

  GLFW_KEY_LEFT = 263;
  GLFW_KEY_ESCAPE = 256;

function initwindow(width, height: LongInt; title: PChar; left, top, dbflag, closeflag: LongInt): LongInt; cdecl; external BgiLib;
procedure closegraph; cdecl; external BgiLib;
procedure setbkcolor(color: LongInt); cdecl; external BgiLib;
procedure setcolor(color: LongInt); cdecl; external BgiLib;
procedure settextstyle(font, direction, charsize: LongInt); cdecl; external BgiLib;
procedure outtextxy(x, y: LongInt; textstring: PChar); cdecl; external BgiLib;
procedure rectangle(left, top, right, bottom: LongInt); cdecl; external BgiLib;
function wxbgi_key_pressed: LongInt; cdecl; external BgiLib;
function wxbgi_read_key: LongInt; cdecl; external BgiLib;
function wxbgi_is_key_down(key: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_should_close: LongInt; cdecl; external BgiLib;
procedure wxbgi_request_close; cdecl; external BgiLib;
function wxbgi_begin_advanced_frame(r, g, b, a: Single; clearColor, clearDepth: LongInt): LongInt; cdecl; external BgiLib;
function wxbgi_end_advanced_frame(swapBuf: LongInt): LongInt; cdecl; external BgiLib;

function DescribeKeyCode(KeyCode: LongInt): string;
begin
  if KeyCode < 0 then
    Exit('none');

  if KeyCode = 0 then
    Exit('0 (extended prefix)');

  if (KeyCode >= 32) and (KeyCode <= 126) then
    Exit(Chr(KeyCode) + ' (' + IntToStr(KeyCode) + ')');

  Result := IntToStr(KeyCode);
end;

function BoolText(Value: Boolean): string;
begin
  if Value then
    Result := 'yes'
  else
    Result := 'no';
end;

procedure DrawTextLine(X, Y: LongInt; const S: string);
begin
  outtextxy(X, Y, PChar(AnsiString(S)));
end;

var
  KeyCode: LongInt;
  LastKeyCode: LongInt;
  LastExtendedCode: LongInt;
  WaitingForExtendedCode: Boolean;
begin
  if initwindow(900, 320, 'wx_BGI Pascal keyboard queue demo', 110, 110, 1, 1) <> 0 then
    Halt(1);

  setbkcolor(BLACK);
  settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);

  LastKeyCode := -1;
  LastExtendedCode := -1;
  WaitingForExtendedCode := False;

  while wxbgi_should_close = 0 do
  begin
    while wxbgi_key_pressed <> 0 do
    begin
      KeyCode := wxbgi_read_key;
      if KeyCode < 0 then
        Break;

      if WaitingForExtendedCode then
      begin
        LastExtendedCode := KeyCode;
        WriteLn('extended scan: ', LastExtendedCode);
        WaitingForExtendedCode := False;
      end
      else
      begin
        LastKeyCode := KeyCode;
          WriteLn('queued key: ', LastKeyCode);
        if KeyCode = 0 then
          WaitingForExtendedCode := True
        else if KeyCode = 27 then
          wxbgi_request_close;
      end;
    end;

    wxbgi_begin_advanced_frame(0.02, 0.04, 0.08, 1.0, 1, 0);

    setcolor(LIGHTGREEN);
    DrawTextLine(20, 20, 'Keyboard queue demo');
    setcolor(WHITE);
    DrawTextLine(20, 50, 'Press normal keys, arrow keys, or Esc to close.');
    DrawTextLine(20, 70, 'Extended keys emit 0 first, then a DOS-style scan code.');
    DrawTextLine(20, 120, 'Last queued key: ' + DescribeKeyCode(LastKeyCode));
    DrawTextLine(20, 145, 'Last extended scan: ' + DescribeKeyCode(LastExtendedCode));
    DrawTextLine(20, 170, 'Esc currently down: ' + BoolText(wxbgi_is_key_down(GLFW_KEY_ESCAPE) = 1));
    DrawTextLine(20, 195, 'Left arrow down: ' + BoolText(wxbgi_is_key_down(GLFW_KEY_LEFT) = 1));

    setcolor(YELLOW);
    rectangle(12, 12, 880, 235);

    wxbgi_end_advanced_frame(1);
    Sleep(16);
  end;

  closegraph;
end.