program DemoBgiWrapperGui;

{$mode objfpc}{$H+}
{$IFDEF MSWINDOWS}
{$APPTYPE GUI}
{$ENDIF}

uses
  SysUtils,
  {$IFDEF MSWINDOWS}
  Windows,
  {$ENDIF}
  Classes;

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

procedure initgraph(graphdriver, graphmode: PLongInt; pathtodriver: PChar); cdecl; external BgiLib;
procedure line(x1, y1, x2, y2: LongInt); cdecl; external BgiLib;
procedure circle(x, y, radius: LongInt); cdecl; external BgiLib;
procedure rectangle(left, top, right, bottom: LongInt); cdecl; external BgiLib;
procedure setcolor(color: LongInt); cdecl; external BgiLib;
procedure outtextxy(x, y: LongInt; textstring: PChar); cdecl; external BgiLib;
procedure floodfill(x, y, border: LongInt); cdecl; external BgiLib;
procedure setfillstyle(pattern, color: LongInt); cdecl; external BgiLib;
function graphresult: LongInt; cdecl; external BgiLib;
procedure closegraph; cdecl; external BgiLib;

procedure PumpWindow(Milliseconds: LongInt);
var
  i, Steps: LongInt;
begin
  if Milliseconds <= 0 then
    Exit;

  Steps := Milliseconds div 16;
  if Steps <= 0 then
    Steps := 1;

  for i := 1 to Steps do
  begin
    line(0, 0, 0, 0);
    Sleep(16);
  end;
end;

procedure ShowInfo(const Msg: string);
begin
  {$IFDEF MSWINDOWS}
  MessageBox(0, PChar(Msg), 'DemoBgiWrapperGui', MB_OK or MB_ICONINFORMATION);
  {$ENDIF}
end;

procedure ShowErrorAndExit(const Msg: string; ExitCode: LongInt);
begin
  {$IFDEF MSWINDOWS}
  MessageBox(0, PChar(Msg), 'DemoBgiWrapperGui Error', MB_OK or MB_ICONERROR);
  {$ENDIF}
  Halt(ExitCode);
end;

var
  gd, gm, ResultCode: LongInt;
begin
  gd := 0;
  gm := 0;

  initgraph(@gd, @gm, nil);
  ResultCode := graphresult;
  if ResultCode <> 0 then
    ShowErrorAndExit('initgraph failed, graphresult=' + IntToStr(ResultCode), 1);

  setcolor(14);
  rectangle(80, 60, 880, 660);

  setcolor(11);
  line(80, 60, 880, 660);
  line(80, 660, 880, 60);

  setcolor(10);
  circle(480, 360, 90);

  setcolor(8);
  circle(480, 360, 210);
  
  setfillstyle(2, 12);
  floodfill(480, 363, 10);

  setcolor(15);
  outtextxy(120, 100, PChar('FREEPASCAL GUI WX BGI DEMO'));
  outtextxy(120, 120, PChar('RECTANGLE LINES CIRCLE AND TEXT'));
  outtextxy(120, 140, PChar('GUI SUBSYSTEM EXECUTABLE'));

  ResultCode := graphresult;
  if ResultCode <> 0 then
    ShowInfo('graphresult=' + IntToStr(ResultCode));

  PumpWindow(5000);
  closegraph;
end.
