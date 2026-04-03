unit Graph;

{$mode objfpc}{$H+}

interface

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
  Detect = 0;
  CGA = 1;
  MCGA = 2;
  EGA = 3;
  EGA64 = 4;
  EGAMONO = 5;
  IBM8514 = 6;
  HERCMONO = 7;
  ATT400 = 8;
  VGA = 9;
  PC3270 = 10;

  IBM8514Hi = 0;

  grOk = 0;
  grNoInitGraph = -1;
  grNotDetected = -2;
  grFileNotFound = -3;
  grInvalidDriver = -4;
  grNoLoadMem = -5;
  grNoScanMem = -6;
  grNoFloodMem = -7;
  grFontNotFound = -8;
  grNoFontMem = -9;
  grInvalidMode = -10;
  grError = -11;
  grIOerror = -12;
  grInvalidFont = -13;
  grInvalidFontNum = -14;
  grInvalidVersion = -18;
  grInitFailed = -19;
  grWindowClosed = -20;
  grInvalidInput = -21;

  BLACK = 0;
  BLUE = 1;
  GREEN = 2;
  CYAN = 3;
  RED = 4;
  MAGENTA = 5;
  BROWN = 6;
  LIGHTGRAY = 7;
  DARKGRAY = 8;
  LIGHTBLUE = 9;
  LIGHTGREEN = 10;
  LIGHTCYAN = 11;
  LIGHTRED = 12;
  LIGHTMAGENTA = 13;
  YELLOW = 14;
  WHITE = 15;

  SolidLn = 0;
  DottedLn = 1;
  CenterLn = 2;
  DashedLn = 3;
  UserBitLn = 4;

  NormWidth = 1;
  ThickWidth = 3;

  EmptyFill = 0;
  SolidFill = 1;
  LineFill = 2;
  LtSlashFill = 3;
  SlashFill = 4;
  BkSlashFill = 5;
  LtBkSlashFill = 6;
  HatchFill = 7;
  XHatchFill = 8;
  InterleaveFill = 9;
  WideDotFill = 10;
  CloseDotFill = 11;
  UserFill = 12;

  CopyPut = 0;
  XORPut = 1;
  ORPut = 2;
  ANDPut = 3;
  NOTPut = 4;

  DefaultFont = 0;
  TriplexFont = 1;
  SmallFont = 2;
  SansSerifFont = 4;
  GothicFont = 8;
  UserCharSize = 1;

  HorizDir = 0;
  VertDir = 1;

  LeftText = 0;
  CenterText = 1;
  RightText = 2;
  BottomText = 0;
  TopText = 2;

  ClipOff = False;
  ClipOn = True;

  TopOff = 0;
  TopOn = 1;

type
  PointType = record
    X: Integer;
    Y: Integer;
  end;

  FillPatternType = array[0..7] of Byte;

  ArcCoordsType = record
    X: Integer;
    Y: Integer;
    XStart: Integer;
    YStart: Integer;
    XEnd: Integer;
    YEnd: Integer;
  end;

  FillSettingsType = record
    Pattern: Integer;
    Color: Integer;
  end;

  LineSettingsType = record
    LineStyle: Integer;
    Pattern: Cardinal;
    Thickness: Integer;
  end;

  TextSettingsType = record
    Font: Integer;
    Direction: Integer;
    CharSize: Integer;
    Horiz: Integer;
    Vert: Integer;
  end;

  ViewPortType = record
    X1: Integer;
    Y1: Integer;
    X2: Integer;
    Y2: Integer;
    Clip: Boolean;
  end;

  PaletteType = record
    Size: Byte;
    Colors: array[0..15] of Integer;
  end;

procedure InitGraph(var GraphDriver, GraphMode: Integer; const PathToDriver: string);
procedure CloseGraph;
function GraphResult: Integer;
function GraphErrorMsg(ErrorCode: Integer): string;
function GetDriverName: string;
function GetModeName(ModeNumber: Integer): string;
function GetGraphMode: Integer;
procedure SetGraphMode(Mode: Integer);
function GetMaxColor: Integer;
function GetMaxX: Integer;
function GetMaxY: Integer;
function GetX: Integer;
function GetY: Integer;
procedure GetAspectRatio(var XAsp, YAsp: Word);
procedure SetAspectRatio(XAsp, YAsp: Word);
procedure GetArcCoords(var ArcInfo: ArcCoordsType);
procedure GetViewSettings(var ViewPort: ViewPortType);
procedure GetLineSettings(var LineInfo: LineSettingsType);
procedure GetFillSettings(var FillInfo: FillSettingsType);
procedure GetTextSettings(var TextInfo: TextSettingsType);
procedure GetPalette(var Palette: PaletteType);
procedure SetAllPalette(const Palette: PaletteType);
procedure SetPalette(ColorNum, Color: Integer);
procedure ClearDevice;
procedure SetColor(Color: Integer);
function GetColor: Integer;
procedure SetBkColor(Color: Integer);
procedure SetLineStyle(LineStyle: Integer; UserPattern: Cardinal; Thickness: Integer);
procedure SetWriteMode(Mode: Integer);
procedure SetFillStyle(Pattern, Color: Integer);
procedure SetFillPattern(const Pattern: FillPatternType; Color: Integer);
procedure SetTextStyle(Font, Direction, CharSize: Integer);
procedure SetTextJustify(Horiz, Vert: Integer);
procedure SetUserCharSize(MultX, DivX, MultY, DivY: Integer);
procedure SetViewPort(Left, Top, Right, Bottom: Integer; Clip: Boolean);
procedure Rectangle(Left, Top, Right, Bottom: Integer);
procedure Bar(Left, Top, Right, Bottom: Integer);
procedure Bar3D(Left, Top, Right, Bottom, Depth, TopFlag: Integer);
procedure Circle(X, Y, Radius: Integer);
procedure Arc(X, Y, StartAngle, EndAngle, Radius: Integer);
procedure Ellipse(X, Y, StartAngle, EndAngle, XRadius, YRadius: Integer);
procedure FillEllipse(X, Y, XRadius, YRadius: Integer);
procedure Sector(X, Y, StartAngle, EndAngle, XRadius, YRadius: Integer);
procedure PieSlice(X, Y, StartAngle, EndAngle, Radius: Integer);
procedure Line(X1, Y1, X2, Y2: Integer);
procedure MoveTo(X, Y: Integer);
procedure MoveRel(DX, DY: Integer);
procedure LineTo(X, Y: Integer);
procedure LineRel(DX, DY: Integer);
procedure PutPixel(X, Y, Color: Integer);
function GetPixel(X, Y: Integer): Integer;
procedure FloodFill(X, Y, Border: Integer);
procedure FillPoly(NumPoints: Integer; const PolyPoints);
procedure OutText(const TextString: string); overload;
procedure OutText(TextChar: Char); overload;
procedure OutTextXY(X, Y: Integer; const TextString: string); overload;
procedure OutTextXY(X, Y: Integer; TextChar: Char); overload;
function TextWidth(const TextString: string): Integer; overload;
function TextWidth(TextChar: Char): Integer; overload;
function TextHeight(const TextString: string): Integer; overload;
function TextHeight(TextChar: Char): Integer; overload;
function ImageSize(Left, Top, Right, Bottom: Integer): Word;
procedure GetImage(Left, Top, Right, Bottom: Integer; var Bitmap);
procedure PutImage(Left, Top: Integer; var Bitmap; Op: Integer);
procedure Delay(MilliSec: Integer);
procedure RestoreCrtMode;
procedure PumpEvents;
function WindowShouldClose: Boolean;
function KeyPressed: Boolean;
function ReadKey: Char;

{ Standalone wx window API -- analogous to wxPython App/Frame/MainLoop }
procedure WxAppCreate;
procedure WxFrameCreate(Width, Height: Integer; const Title: string);
procedure WxAppMainLoop;
procedure WxCloseFrame;
procedure WxCloseAfterMs(Ms: Integer);
procedure WxSetFrameRate(Fps: Integer);
procedure WxRefresh;

implementation

type
  TNativeArcCoords = record
    X: LongInt;
    Y: LongInt;
    XStart: LongInt;
    YStart: LongInt;
    XEnd: LongInt;
    YEnd: LongInt;
  end;

  TNativeFillSettings = record
    Pattern: LongInt;
    Color: LongInt;
  end;

  TNativeLineSettings = record
    LineStyle: LongInt;
    Pattern: Cardinal;
    Thickness: LongInt;
  end;

  TNativeTextSettings = record
    Font: LongInt;
    Direction: LongInt;
    CharSize: LongInt;
    Horiz: LongInt;
    Vert: LongInt;
  end;

  TNativeViewPort = record
    X1: LongInt;
    Y1: LongInt;
    X2: LongInt;
    Y2: LongInt;
    Clip: LongInt;
  end;

  TNativePalette = record
    Size: Byte;
    Colors: array[0..15] of LongInt;
  end;

  PNativeArcCoords = ^TNativeArcCoords;
  PNativeFillSettings = ^TNativeFillSettings;
  PNativeLineSettings = ^TNativeLineSettings;
  PNativeTextSettings = ^TNativeTextSettings;
  PNativeViewPort = ^TNativeViewPort;
  PNativePalette = ^TNativePalette;

var
  LastGraphMode: Integer = 0;
  LastWindowWidth: Integer = 960;
  LastWindowHeight: Integer = 720;
  LastWindowLeft: Integer = 64;
  LastWindowTop: Integer = 64;
  LastWindowTitle: string = 'wx_BGI Pascal Demo';
  WindowExpectedOpen: Boolean = False;

procedure _initgraph(GraphDriver, GraphMode: PLongInt; PathToDriver: PChar); cdecl; external BgiLib name 'initgraph';
function _initwindow(Width, Height: LongInt; Title: PChar; Left, Top, DbFlag, CloseFlag: LongInt): LongInt; cdecl; external BgiLib name 'initwindow';
procedure _closegraph; cdecl; external BgiLib name 'closegraph';
function _graphresult: LongInt; cdecl; external BgiLib name 'graphresult';
function _grapherrormsg(ErrorCode: LongInt): PChar; cdecl; external BgiLib name 'grapherrormsg';
function _getdrivername: PChar; cdecl; external BgiLib name 'getdrivername';
function _getmodename(ModeNumber: LongInt): PChar; cdecl; external BgiLib name 'getmodename';
function _getgraphmode: LongInt; cdecl; external BgiLib name 'getgraphmode';
procedure _setgraphmode(Mode: LongInt); cdecl; external BgiLib name 'setgraphmode';
function _getmaxcolor: LongInt; cdecl; external BgiLib name 'getmaxcolor';
function _getmaxx: LongInt; cdecl; external BgiLib name 'getmaxx';
function _getmaxy: LongInt; cdecl; external BgiLib name 'getmaxy';
function _getx: LongInt; cdecl; external BgiLib name 'getx';
function _gety: LongInt; cdecl; external BgiLib name 'gety';
procedure _getaspectratio(XAsp, YAsp: PLongInt); cdecl; external BgiLib name 'getaspectratio';
procedure _setaspectratio(XAsp, YAsp: LongInt); cdecl; external BgiLib name 'setaspectratio';
procedure _getarccoords(ArcInfo: PNativeArcCoords); cdecl; external BgiLib name 'getarccoords';
procedure _getviewsettings(ViewPort: PNativeViewPort); cdecl; external BgiLib name 'getviewsettings';
procedure _getlinesettings(LineInfo: PNativeLineSettings); cdecl; external BgiLib name 'getlinesettings';
procedure _getfillsettings(FillInfo: PNativeFillSettings); cdecl; external BgiLib name 'getfillsettings';
procedure _gettextsettings(TextInfo: PNativeTextSettings); cdecl; external BgiLib name 'gettextsettings';
procedure _getpalette(Palette: PNativePalette); cdecl; external BgiLib name 'getpalette';
procedure _setallpalette(Palette: PNativePalette); cdecl; external BgiLib name 'setallpalette';
procedure _setpalette(ColorNum, Color: LongInt); cdecl; external BgiLib name 'setpalette';
procedure _cleardevice; cdecl; external BgiLib name 'cleardevice';
procedure _setcolor(Color: LongInt); cdecl; external BgiLib name 'setcolor';
function _getcolor: LongInt; cdecl; external BgiLib name 'getcolor';
procedure _setbkcolor(Color: LongInt); cdecl; external BgiLib name 'setbkcolor';
procedure _setlinestyle(LineStyle: LongInt; UserPattern: Cardinal; Thickness: LongInt); cdecl; external BgiLib name 'setlinestyle';
procedure _setwritemode(Mode: LongInt); cdecl; external BgiLib name 'setwritemode';
procedure _setfillstyle(Pattern, Color: LongInt); cdecl; external BgiLib name 'setfillstyle';
procedure _setfillpattern(Pattern: PChar; Color: LongInt); cdecl; external BgiLib name 'setfillpattern';
procedure _settextstyle(Font, Direction, CharSize: LongInt); cdecl; external BgiLib name 'settextstyle';
procedure _settextjustify(Horiz, Vert: LongInt); cdecl; external BgiLib name 'settextjustify';
procedure _setusercharsize(MultX, DivX, MultY, DivY: LongInt); cdecl; external BgiLib name 'setusercharsize';
procedure _setviewport(Left, Top, Right, Bottom, Clip: LongInt); cdecl; external BgiLib name 'setviewport';
procedure _rectangle(Left, Top, Right, Bottom: LongInt); cdecl; external BgiLib name 'rectangle';
procedure _bar(Left, Top, Right, Bottom: LongInt); cdecl; external BgiLib name 'bar';
procedure _bar3d(Left, Top, Right, Bottom, Depth, TopFlag: LongInt); cdecl; external BgiLib name 'bar3d';
procedure _circle(X, Y, Radius: LongInt); cdecl; external BgiLib name 'circle';
procedure _arc(X, Y, StartAngle, EndAngle, Radius: LongInt); cdecl; external BgiLib name 'arc';
procedure _ellipse(X, Y, StartAngle, EndAngle, XRadius, YRadius: LongInt); cdecl; external BgiLib name 'ellipse';
procedure _fillellipse(X, Y, XRadius, YRadius: LongInt); cdecl; external BgiLib name 'fillellipse';
procedure _sector(X, Y, StartAngle, EndAngle, XRadius, YRadius: LongInt); cdecl; external BgiLib name 'sector';
procedure _pieslice(X, Y, StartAngle, EndAngle, Radius: LongInt); cdecl; external BgiLib name 'pieslice';
procedure _line(X1, Y1, X2, Y2: LongInt); cdecl; external BgiLib name 'line';
procedure _moveto(X, Y: LongInt); cdecl; external BgiLib name 'moveto';
procedure _moverel(DX, DY: LongInt); cdecl; external BgiLib name 'moverel';
procedure _lineto(X, Y: LongInt); cdecl; external BgiLib name 'lineto';
procedure _linerel(DX, DY: LongInt); cdecl; external BgiLib name 'linerel';
procedure _putpixel(X, Y, Color: LongInt); cdecl; external BgiLib name 'putpixel';
function _getpixel(X, Y: LongInt): LongInt; cdecl; external BgiLib name 'getpixel';
procedure _floodfill(X, Y, Border: LongInt); cdecl; external BgiLib name 'floodfill';
procedure _fillpoly(NumPoints: LongInt; PolyPoints: PLongInt); cdecl; external BgiLib name 'fillpoly';
procedure _outtext(TextString: PChar); cdecl; external BgiLib name 'outtext';
procedure _outtextxy(X, Y: LongInt; TextString: PChar); cdecl; external BgiLib name 'outtextxy';
function _textwidth(TextString: PChar): LongInt; cdecl; external BgiLib name 'textwidth';
function _textheight(TextString: PChar): LongInt; cdecl; external BgiLib name 'textheight';
function _imagesize(Left, Top, Right, Bottom: LongInt): Cardinal; cdecl; external BgiLib name 'imagesize';
procedure _getimage(Left, Top, Right, Bottom: LongInt; Bitmap: Pointer); cdecl; external BgiLib name 'getimage';
procedure _putimage(Left, Top: LongInt; Bitmap: Pointer; Op: LongInt); cdecl; external BgiLib name 'putimage';
procedure _restorecrtmode; cdecl; external BgiLib name 'restorecrtmode';
function _wxbgi_is_ready: LongInt; cdecl; external BgiLib name 'wxbgi_is_ready';
function _wxbgi_poll_events: LongInt; cdecl; external BgiLib name 'wxbgi_poll_events';
function _wxbgi_key_pressed: LongInt; cdecl; external BgiLib name 'wxbgi_key_pressed';
function _wxbgi_read_key: LongInt; cdecl; external BgiLib name 'wxbgi_read_key';
function _wxbgi_should_close: LongInt; cdecl; external BgiLib name 'wxbgi_should_close';
procedure _wxbgi_wx_app_create; cdecl; external BgiLib name 'wxbgi_wx_app_create';
procedure _wxbgi_wx_frame_create(Width, Height: LongInt; Title: PChar); cdecl; external BgiLib name 'wxbgi_wx_frame_create';
procedure _wxbgi_wx_app_main_loop; cdecl; external BgiLib name 'wxbgi_wx_app_main_loop';
procedure _wxbgi_wx_close_frame; cdecl; external BgiLib name 'wxbgi_wx_close_frame';
procedure _wxbgi_wx_close_after_ms(Ms: LongInt); cdecl; external BgiLib name 'wxbgi_wx_close_after_ms';
procedure _wxbgi_wx_set_frame_rate(Fps: LongInt); cdecl; external BgiLib name 'wxbgi_wx_set_frame_rate';
procedure _wxbgi_wx_refresh; cdecl; external BgiLib name 'wxbgi_wx_refresh';

function SafeString(Value: PChar): string;
begin
  if Value = nil then
    Exit('');
  Result := string(Value);
end;

function EnsureWindowOpen: Boolean;
begin
  if WindowExpectedOpen and (_wxbgi_is_ready <> 0) then
    Exit(True);

  if _initwindow(LastWindowWidth, LastWindowHeight, PChar(AnsiString(LastWindowTitle)), LastWindowLeft, LastWindowTop, 0, 1) = 0 then
  begin
    WindowExpectedOpen := True;
    Result := True;
  end
  else
    Result := False;
end;

procedure InitGraph(var GraphDriver, GraphMode: Integer; const PathToDriver: string);
var
  DriverValue: LongInt;
  ModeValue: LongInt;
  DriverPath: AnsiString;
begin
  DriverValue := GraphDriver;
  ModeValue := GraphMode;
  DriverPath := AnsiString(PathToDriver);
  _initgraph(@DriverValue, @ModeValue, PChar(DriverPath));
  if GraphResult = grOk then
  begin
    GraphDriver := VGA;
    GraphMode := 0;
    LastGraphMode := GraphMode;
    WindowExpectedOpen := True;
  end
  else
  begin
    GraphDriver := Integer(DriverValue);
    GraphMode := Integer(ModeValue);
  end;
end;

procedure CloseGraph;
begin
  _closegraph;
  WindowExpectedOpen := False;
end;

function GraphResult: Integer;
begin
  Result := _graphresult;
end;

function GraphErrorMsg(ErrorCode: Integer): string;
begin
  Result := SafeString(_grapherrormsg(ErrorCode));
end;

function GetDriverName: string;
begin
  Result := SafeString(_getdrivername);
end;

function GetModeName(ModeNumber: Integer): string;
begin
  Result := SafeString(_getmodename(ModeNumber));
end;

function GetGraphMode: Integer;
begin
  Result := _getgraphmode;
end;

procedure SetGraphMode(Mode: Integer);
begin
  LastGraphMode := Mode;
  if EnsureWindowOpen then
    _setgraphmode(Mode);
end;

function GetMaxColor: Integer;
begin
  Result := _getmaxcolor;
end;

function GetMaxX: Integer;
begin
  Result := _getmaxx;
end;

function GetMaxY: Integer;
begin
  Result := _getmaxy;
end;

function GetX: Integer;
begin
  Result := _getx;
end;

function GetY: Integer;
begin
  Result := _gety;
end;

procedure GetAspectRatio(var XAsp, YAsp: Word);
var
  NativeX: LongInt;
  NativeY: LongInt;
begin
  _getaspectratio(@NativeX, @NativeY);
  XAsp := Word(NativeX);
  YAsp := Word(NativeY);
end;

procedure SetAspectRatio(XAsp, YAsp: Word);
begin
  _setaspectratio(XAsp, YAsp);
end;

procedure GetArcCoords(var ArcInfo: ArcCoordsType);
var
  NativeArc: TNativeArcCoords;
begin
  _getarccoords(@NativeArc);
  ArcInfo.X := NativeArc.X;
  ArcInfo.Y := NativeArc.Y;
  ArcInfo.XStart := NativeArc.XStart;
  ArcInfo.YStart := NativeArc.YStart;
  ArcInfo.XEnd := NativeArc.XEnd;
  ArcInfo.YEnd := NativeArc.YEnd;
end;

procedure GetViewSettings(var ViewPort: ViewPortType);
var
  NativeViewPort: TNativeViewPort;
begin
  _getviewsettings(@NativeViewPort);
  ViewPort.X1 := NativeViewPort.X1;
  ViewPort.Y1 := NativeViewPort.Y1;
  ViewPort.X2 := NativeViewPort.X2;
  ViewPort.Y2 := NativeViewPort.Y2;
  ViewPort.Clip := NativeViewPort.Clip <> 0;
end;

procedure GetLineSettings(var LineInfo: LineSettingsType);
var
  NativeLineInfo: TNativeLineSettings;
begin
  _getlinesettings(@NativeLineInfo);
  LineInfo.LineStyle := NativeLineInfo.LineStyle;
  LineInfo.Pattern := NativeLineInfo.Pattern;
  LineInfo.Thickness := NativeLineInfo.Thickness;
end;

procedure GetFillSettings(var FillInfo: FillSettingsType);
var
  NativeFillInfo: TNativeFillSettings;
begin
  _getfillsettings(@NativeFillInfo);
  FillInfo.Pattern := NativeFillInfo.Pattern;
  FillInfo.Color := NativeFillInfo.Color;
end;

procedure GetTextSettings(var TextInfo: TextSettingsType);
var
  NativeTextInfo: TNativeTextSettings;
begin
  _gettextsettings(@NativeTextInfo);
  TextInfo.Font := NativeTextInfo.Font;
  TextInfo.Direction := NativeTextInfo.Direction;
  TextInfo.CharSize := NativeTextInfo.CharSize;
  TextInfo.Horiz := NativeTextInfo.Horiz;
  TextInfo.Vert := NativeTextInfo.Vert;
end;

procedure GetPalette(var Palette: PaletteType);
var
  NativePalette: TNativePalette;
  Index: Integer;
begin
  _getpalette(@NativePalette);
  Palette.Size := NativePalette.Size;
  for Index := Low(Palette.Colors) to High(Palette.Colors) do
    Palette.Colors[Index] := NativePalette.Colors[Index];
end;

procedure SetAllPalette(const Palette: PaletteType);
var
  NativePalette: TNativePalette;
  Index: Integer;
begin
  NativePalette.Size := Palette.Size;
  for Index := Low(Palette.Colors) to High(Palette.Colors) do
    NativePalette.Colors[Index] := Palette.Colors[Index];
  _setallpalette(@NativePalette);
end;

procedure SetPalette(ColorNum, Color: Integer);
begin
  _setpalette(ColorNum, Color);
end;

procedure ClearDevice;
begin
  _cleardevice;
end;

procedure SetColor(Color: Integer);
begin
  _setcolor(Color);
end;

function GetColor: Integer;
begin
  Result := _getcolor;
end;

procedure SetBkColor(Color: Integer);
begin
  _setbkcolor(Color);
end;

procedure SetLineStyle(LineStyle: Integer; UserPattern: Cardinal; Thickness: Integer);
begin
  _setlinestyle(LineStyle, UserPattern, Thickness);
end;

procedure SetWriteMode(Mode: Integer);
begin
  _setwritemode(Mode);
end;

procedure SetFillStyle(Pattern, Color: Integer);
begin
  _setfillstyle(Pattern, Color);
end;

procedure SetFillPattern(const Pattern: FillPatternType; Color: Integer);
begin
  _setfillpattern(PChar(@Pattern[0]), Color);
end;

procedure SetTextStyle(Font, Direction, CharSize: Integer);
begin
  _settextstyle(Font, Direction, CharSize);
end;

procedure SetTextJustify(Horiz, Vert: Integer);
begin
  _settextjustify(Horiz, Vert);
end;

procedure SetUserCharSize(MultX, DivX, MultY, DivY: Integer);
begin
  _setusercharsize(MultX, DivX, MultY, DivY);
end;

procedure SetViewPort(Left, Top, Right, Bottom: Integer; Clip: Boolean);
begin
  _setviewport(Left, Top, Right, Bottom, Ord(Clip));
end;

procedure Rectangle(Left, Top, Right, Bottom: Integer);
begin
  _rectangle(Left, Top, Right, Bottom);
end;

procedure Bar(Left, Top, Right, Bottom: Integer);
begin
  _bar(Left, Top, Right, Bottom);
end;

procedure Bar3D(Left, Top, Right, Bottom, Depth, TopFlag: Integer);
begin
  _bar3d(Left, Top, Right, Bottom, Depth, TopFlag);
end;

procedure Circle(X, Y, Radius: Integer);
begin
  _circle(X, Y, Radius);
end;

procedure Arc(X, Y, StartAngle, EndAngle, Radius: Integer);
begin
  _arc(X, Y, StartAngle, EndAngle, Radius);
end;

procedure Ellipse(X, Y, StartAngle, EndAngle, XRadius, YRadius: Integer);
begin
  _ellipse(X, Y, StartAngle, EndAngle, XRadius, YRadius);
end;

procedure FillEllipse(X, Y, XRadius, YRadius: Integer);
begin
  _fillellipse(X, Y, XRadius, YRadius);
end;

procedure Sector(X, Y, StartAngle, EndAngle, XRadius, YRadius: Integer);
begin
  _sector(X, Y, StartAngle, EndAngle, XRadius, YRadius);
end;

procedure PieSlice(X, Y, StartAngle, EndAngle, Radius: Integer);
begin
  _pieslice(X, Y, StartAngle, EndAngle, Radius);
end;

procedure Line(X1, Y1, X2, Y2: Integer);
begin
  _line(X1, Y1, X2, Y2);
end;

procedure MoveTo(X, Y: Integer);
begin
  _moveto(X, Y);
end;

procedure MoveRel(DX, DY: Integer);
begin
  _moverel(DX, DY);
end;

procedure LineTo(X, Y: Integer);
begin
  _lineto(X, Y);
end;

procedure LineRel(DX, DY: Integer);
begin
  _linerel(DX, DY);
end;

procedure PutPixel(X, Y, Color: Integer);
begin
  _putpixel(X, Y, Color);
end;

function GetPixel(X, Y: Integer): Integer;
begin
  Result := _getpixel(X, Y);
end;

procedure FloodFill(X, Y, Border: Integer);
begin
  _floodfill(X, Y, Border);
end;

procedure FillPoly(NumPoints: Integer; const PolyPoints);
var
  SourcePoints: ^PointType;
  NativePoints: array of LongInt;
  Index: Integer;
begin
  if NumPoints <= 0 then
    Exit;

  SourcePoints := @PolyPoints;
  SetLength(NativePoints, NumPoints * 2);
  for Index := 0 to NumPoints - 1 do
  begin
    NativePoints[Index * 2] := SourcePoints[Index].X;
    NativePoints[Index * 2 + 1] := SourcePoints[Index].Y;
  end;
  _fillpoly(NumPoints, @NativePoints[0]);
end;

procedure OutText(const TextString: string);
begin
  _outtext(PChar(AnsiString(TextString)));
end;

procedure OutText(TextChar: Char);
begin
  OutText(string(TextChar));
end;

procedure OutTextXY(X, Y: Integer; const TextString: string);
begin
  _outtextxy(X, Y, PChar(AnsiString(TextString)));
end;

procedure OutTextXY(X, Y: Integer; TextChar: Char);
begin
  OutTextXY(X, Y, string(TextChar));
end;

function TextWidth(const TextString: string): Integer;
begin
  Result := _textwidth(PChar(AnsiString(TextString)));
end;

function TextWidth(TextChar: Char): Integer;
begin
  Result := TextWidth(string(TextChar));
end;

function TextHeight(const TextString: string): Integer;
begin
  Result := _textheight(PChar(AnsiString(TextString)));
end;

function TextHeight(TextChar: Char): Integer;
begin
  Result := TextHeight(string(TextChar));
end;

function ImageSize(Left, Top, Right, Bottom: Integer): Word;
begin
  Result := Word(_imagesize(Left, Top, Right, Bottom));
end;

procedure GetImage(Left, Top, Right, Bottom: Integer; var Bitmap);
begin
  _getimage(Left, Top, Right, Bottom, @Bitmap);
end;

procedure PutImage(Left, Top: Integer; var Bitmap; Op: Integer);
begin
  _putimage(Left, Top, @Bitmap, Op);
end;

procedure Delay(MilliSec: Integer);
var
  Remaining: Integer;
  Slice: Integer;
begin
  Remaining := MilliSec;
  while Remaining > 0 do
  begin
    PumpEvents;
    if WindowShouldClose then
      Exit;

    if Remaining > 16 then
      Slice := 16
    else
      Slice := Remaining;
    Sleep(Slice);
    Dec(Remaining, Slice);
  end;
end;

procedure RestoreCrtMode;
begin
  _restorecrtmode;
  { Keep WindowExpectedOpen = True so KeyPressed/ReadKey still work via the
    keyboard queue.  The window stays alive; only an inCrtMode flag is set. }
end;

procedure PumpEvents;
begin
  if WindowExpectedOpen and (_wxbgi_is_ready <> 0) then
    _wxbgi_poll_events;
end;

function WindowShouldClose: Boolean;
begin
  Result := WindowExpectedOpen and (_wxbgi_should_close <> 0);
end;

function KeyPressed: Boolean;
begin
  PumpEvents;
  Result := WindowExpectedOpen and (_wxbgi_key_pressed <> 0);
end;

function ReadKey: Char;
var
  KeyCode: LongInt;
begin
  repeat
    PumpEvents;
    if WindowShouldClose then
      Halt(0);

    KeyCode := _wxbgi_read_key;
    if KeyCode >= 0 then
      Exit(Char(Byte(KeyCode and $FF)));

    Sleep(16);
  until False;
end;

procedure WxAppCreate;
begin
  _wxbgi_wx_app_create;
end;

procedure WxFrameCreate(Width, Height: Integer; const Title: string);
begin
  _wxbgi_wx_frame_create(Width, Height, PChar(AnsiString(Title)));
  WindowExpectedOpen := True;
end;

procedure WxAppMainLoop;
begin
  _wxbgi_wx_app_main_loop;
  WindowExpectedOpen := False;
end;

procedure WxCloseFrame;
begin
  _wxbgi_wx_close_frame;
end;

procedure WxCloseAfterMs(Ms: Integer);
begin
  _wxbgi_wx_close_after_ms(Ms);
end;

procedure WxSetFrameRate(Fps: Integer);
begin
  _wxbgi_wx_set_frame_rate(Fps);
end;

procedure WxRefresh;
begin
  _wxbgi_wx_refresh;
end;

end.