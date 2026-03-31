# Keyboard and Mouse Event Handling

This document explains how wx_BGI_Graphics captures, translates, queues, and exposes keyboard and mouse input to the programmer. It covers the low-level GLFW background APIs used, the data structures involved, the threading model, and a full code map of every relevant function.

## Table of Contents

1. [Overview](#overview)
2. [Low-Level Backend APIs](#low-level-backend-apis)
3. [Keyboard Handling](#keyboard-handling)
   - [GLFW Callbacks](#keyboard-glfw-callbacks)
   - [Key Translation and Queue](#key-translation-and-queue)
   - [DOS-Style Extended Key Codes](#dos-style-extended-key-codes)
   - [BgiState Keyboard Fields](#bgistate-keyboard-fields)
   - [Public Keyboard API](#public-keyboard-api)
   - [Test Seams](#test-seams)
4. [Mouse Handling](#mouse-handling)
   - [GLFW Callbacks](#mouse-glfw-callbacks)
   - [Coordinate System](#coordinate-system)
   - [BgiState Mouse Fields](#bgistate-mouse-fields)
   - [Public Mouse API](#public-mouse-api)
   - [Click-to-Pick Pipeline](#click-to-pick-pipeline)
5. [Event Pump](#event-pump)
6. [Thread Safety](#thread-safety)
7. [Code Map](#code-map)
   - [Data Structures](#data-structures)
   - [Keyboard Layer](#keyboard-layer)
   - [Mouse Layer](#mouse-layer)
   - [Event Pump Layer](#event-pump-layer)
   - [File Dependency Diagram](#file-dependency-diagram)

---

## Overview

Input handling in wx_BGI_Graphics is a **synchronous, single-threaded, callback-driven pipeline** built on top of GLFW. The programmer drives the input loop by calling `wxbgi_poll_events()` (or the frame helpers) from their main loop. GLFW then fires any registered callbacks synchronously on that same thread. Keyboard events are buffered in an in-memory queue; mouse position is tracked in plain integer fields; mouse button clicks directly trigger the object pick/selection system.

```
Programmer main loop
    |
    v
wxbgi_poll_events()   <-- acquires gMutex, calls glfwPollEvents()
    |
    +-- [key event]    --> keyCallback()    --> gState.keyDown[], gState.keyQueue
    +-- [char event]   --> charCallback()   --> gState.keyQueue
    +-- [cursor move]  --> cursorPosCallback() --> gState.mouseX/Y, mouseMoved
    +-- [mouse click]  --> mouseButtonCallback() --> overlayPerformPick()
                                                       --> gState.selectedObjectIds
```

---

## Low-Level Backend APIs

The library relies exclusively on **GLFW 3.4** for window management and raw input events. No platform-specific input APIs (Win32 `GetAsyncKeyState`, X11 `XNextEvent`, Cocoa `NSEvent`) are used directly — GLFW abstracts them.

| GLFW API | Purpose |
|---|---|
| `glfwSetKeyCallback(window, cb)` | Registers the keyboard key callback |
| `glfwSetCharCallback(window, cb)` | Registers the Unicode character callback |
| `glfwSetCursorPosCallback(window, cb)` | Registers the mouse position callback |
| `glfwSetMouseButtonCallback(window, cb)` | Registers the mouse button callback |
| `glfwPollEvents()` | Processes all pending OS events; fires registered callbacks synchronously |
| `glfwWindowShouldClose(window)` | Checks whether the user has closed the window |

**Callback signatures** (as required by GLFW):

```cpp
// Key press / repeat / release
void keyCallback(GLFWwindow*, int key, int scancode, int action, int mods);

// Unicode character input (printable chars only)
void charCallback(GLFWwindow*, unsigned int codepoint);

// Mouse cursor position (in window pixel space, top-left origin)
void cursorPosCallback(GLFWwindow*, double xpos, double ypos);

// Mouse button press / release
void mouseButtonCallback(GLFWwindow*, int button, int action, int mods);
```

All four callbacks are registered in `initializeWindow()` immediately after `glewInit()`:

```cpp
// src/bgi_api.cpp, lines 224-227
glfwSetKeyCallback(bgi::gState.window, keyCallback);
glfwSetCharCallback(bgi::gState.window, charCallback);
glfwSetCursorPosCallback(bgi::gState.window, cursorPosCallback);
glfwSetMouseButtonCallback(bgi::gState.window, mouseButtonCallback);
```

---

## Keyboard Handling

### Keyboard GLFW Callbacks

Two GLFW callbacks cooperate to cover the full range of keyboard input:

#### `keyCallback` — special keys (`src/bgi_api.cpp`, lines 36–136)

Fires on every key press, repeat, and release.

- **On press or repeat:** Translates GLFW key codes for special keys (arrows, F-keys, navigation) into DOS-compatible scancodes and pushes them into `gState.keyQueue`. Control characters (Escape, Enter, Tab, Backspace) are pushed as single byte codes.
- **On any action:** Updates `gState.keyDown[key]` (1 = pressed/repeat, 0 = released) for every GLFW key. This is the source of truth for `wxbgi_is_key_down()`.
- Printable characters are **not** handled here — they are delegated to `charCallback` to avoid double-queuing.

#### `charCallback` — printable characters (`src/bgi_api.cpp`, lines 138–152)

Fires only for printable input (letter, digit, symbol keys, including with Shift/Caps).

- Accepts Unicode codepoints 1–255.
- Silently drops codepoints > 255 (non-Latin characters unsupported in classic BGI).
- Silently drops Tab (9), Enter (13), and Escape (27) — those are already handled by `keyCallback`.
- Pushes the codepoint as-is into `gState.keyQueue`.

**Why two callbacks?** GLFW guarantees that `charCallback` produces the correct OS-composed character (honouring keyboard layout, Shift, dead keys, IME). Using it for printable characters and `keyCallback` for control/special keys avoids duplication and correctly handles international layouts.

---

### Key Translation and Queue

#### Queue helpers (`src/bgi_api.cpp`, lines 25–34, anonymous namespace)

```cpp
void queueKeyCode(int keyCode)
{
    bgi::gState.keyQueue.push(keyCode);
}

void queueExtendedKey(int scanCode)
{
    queueKeyCode(0);         // DOS extended key prefix
    queueKeyCode(scanCode);  // DOS scancode
}
```

`queueExtendedKey()` produces the classic two-byte sequence `{0, scancode}` that DOS programs expect for non-printable keys.

#### Storage

```cpp
// src/bgi_types.h, line 395
std::queue<int> keyQueue;

// src/bgi_types.h, line 396
std::array<std::uint8_t, 512> keyDown{};
```

- `keyQueue` — unbounded FIFO of translated key codes (integers 0–255).
- `keyDown[glfwKey]` — instantaneous press state; index is the raw GLFW key constant.

---

### DOS-Style Extended Key Codes

Classic Borland BGI programs read the keyboard with `getch()`, which returns 0 for extended keys and then returns the scancode on the next call. This library emulates that behaviour exactly by pushing **two** integers for each special key.

| Key | GLFW constant | Scancode | Queue sequence |
|---|---|---|---|
| Up arrow | `GLFW_KEY_UP` | 72 | `0, 72` |
| Down arrow | `GLFW_KEY_DOWN` | 80 | `0, 80` |
| Left arrow | `GLFW_KEY_LEFT` | 75 | `0, 75` |
| Right arrow | `GLFW_KEY_RIGHT` | 77 | `0, 77` |
| Home | `GLFW_KEY_HOME` | 71 | `0, 71` |
| End | `GLFW_KEY_END` | 79 | `0, 79` |
| Page Up | `GLFW_KEY_PAGE_UP` | 73 | `0, 73` |
| Page Down | `GLFW_KEY_PAGE_DOWN` | 81 | `0, 81` |
| Insert | `GLFW_KEY_INSERT` | 82 | `0, 82` |
| Delete | `GLFW_KEY_DELETE` | 83 | `0, 83` |
| F1 | `GLFW_KEY_F1` | 59 | `0, 59` |
| F2 | `GLFW_KEY_F2` | 60 | `0, 60` |
| F3 | `GLFW_KEY_F3` | 61 | `0, 61` |
| F4 | `GLFW_KEY_F4` | 62 | `0, 62` |
| F5 | `GLFW_KEY_F5` | 63 | `0, 63` |
| F6 | `GLFW_KEY_F6` | 64 | `0, 64` |
| F7 | `GLFW_KEY_F7` | 65 | `0, 65` |
| F8 | `GLFW_KEY_F8` | 66 | `0, 66` |
| F9 | `GLFW_KEY_F9` | 67 | `0, 67` |
| F10 | `GLFW_KEY_F10` | 68 | `0, 68` |
| F11 | `GLFW_KEY_F11` | 133 | `0, 133` |
| F12 | `GLFW_KEY_F12` | 134 | `0, 134` |

Single-byte control characters (no prefix):

| Key | Code |
|---|---|
| Escape | 27 |
| Enter / KP Enter | 13 |
| Tab | 9 |
| Backspace | 8 |

---

### BgiState Keyboard Fields

Defined in `src/bgi_types.h`, struct `BgiState`:

```cpp
std::queue<int>               keyQueue;    // Translated key codes awaiting wxbgi_read_key()
std::array<std::uint8_t, 512> keyDown{};  // keyDown[GLFW_KEY_*] = 1 if held, 0 if released
```

---

### Public Keyboard API

All public keyboard functions are declared in `src/wx_bgi_ext.h` and implemented in `src/bgi_modern_api.cpp`. Each acquires `gMutex` via `std::lock_guard`.

#### `wxbgi_key_pressed()` — lines 75–85

```cpp
BGI_API int BGI_CALL wxbgi_key_pressed(void);
```

Returns 1 if at least one key code is waiting in `keyQueue`, 0 if empty. Does not consume the queued code. Equivalent to the DOS `kbhit()` function.

**Usage pattern:**
```cpp
while (wxbgi_key_pressed()) {
    int k = wxbgi_read_key();
    // handle k
}
```

#### `wxbgi_read_key()` — lines 87–105

```cpp
BGI_API int BGI_CALL wxbgi_read_key(void);
```

Dequeues and returns the next translated key code (0–255), or -1 if the queue is empty. For extended keys, the first call returns 0 and the next call returns the scancode.

**Usage pattern:**
```cpp
int k = wxbgi_read_key();
if (k == 0) {
    int scan = wxbgi_read_key();  // e.g. 72 = Up arrow
}
```

#### `wxbgi_is_key_down(int key)` — lines 107–118

```cpp
BGI_API int BGI_CALL wxbgi_is_key_down(int key);
```

Queries the **instantaneous** press state of a key by its GLFW constant. Returns 1 if currently held, 0 if released, -1 if `key` is out of range. This bypasses the queue — useful for detecting held modifier or navigation keys without consuming queue entries.

**Usage pattern:**
```cpp
if (wxbgi_is_key_down(GLFW_KEY_LEFT_SHIFT)) {
    // Shift is currently held
}
```

---

### Test Seams

When compiled with `WXBGI_ENABLE_TEST_SEAMS`, three additional functions are available for CI keyboard injection:

| Function | Lines | Purpose |
|---|---|---|
| `wxbgi_test_clear_key_queue()` | 136–150 | Empties `keyQueue` |
| `wxbgi_test_inject_key_code(int)` | 152–164 | Pushes one code (0–255) into `keyQueue` |
| `wxbgi_test_inject_extended_scan(int)` | 166–179 | Pushes `{0, scanCode}` into `keyQueue` |

These functions acquire `gMutex` and directly manipulate `gState.keyQueue`, simulating what the GLFW callbacks would do during real key presses.

---

## Mouse Handling

### Mouse GLFW Callbacks

#### `cursorPosCallback` — (`src/bgi_api.cpp`, lines 154–160)

Fires on every mouse movement inside the window.

```cpp
void cursorPosCallback(GLFWwindow*, double xpos, double ypos)
{
    bgi::gState.mouseX     = static_cast<int>(xpos);
    bgi::gState.mouseY     = static_cast<int>(ypos);
    bgi::gState.mouseMoved = true;
}
```

- Converts GLFW's `double` position to `int` pixels.
- Sets `mouseMoved = true`; the flag is cleared by `wxbgi_mouse_moved()` on the next API call.
- **No mutex acquired** — this callback fires while `gMutex` is already held by the event pump.

#### `mouseButtonCallback` — (`src/bgi_api.cpp`, lines 162–175)

Fires on mouse button press and release.

```cpp
void mouseButtonCallback(GLFWwindow*, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
        bgi::overlayPerformPick(bgi::gState.mouseX, bgi::gState.mouseY, ctrl);
    }
}
```

Currently only the **left mouse button press** is acted upon. Right button, middle button, and release events are accepted by GLFW but not processed. The CTRL modifier enables multi-select mode for the pick/selection system.

> **Note — scroll wheel:** `glfwSetScrollCallback` is **not** registered. Scroll events are not captured in the current implementation.

---

### Coordinate System

GLFW delivers mouse positions in **window-pixel space** with the origin at the **top-left corner** of the client area:

```
(0,0) -----> +X (right)
  |
  v
+Y (down)
```

These coordinates match the BGI pixel coordinate system directly — no transformation is applied. `gState.mouseX` and `gState.mouseY` are ready to use for BGI-pixel-space hit tests.

---

### BgiState Mouse Fields

Defined in `src/bgi_types.h`, struct `BgiState`:

```cpp
int  mouseX{0};         // Current cursor X in window pixels (top-left = 0,0)
int  mouseY{0};         // Current cursor Y in window pixels
bool mouseMoved{false}; // True after any cursor move; cleared by wxbgi_mouse_moved()
```

Selection state (used by the click-to-pick pipeline):

```cpp
std::vector<std::string> selectedObjectIds;    // IDs of currently selected DDS objects
int selectionFlashScheme{0};                   // 0 = orange flash, 1 = purple flash
int selectionPickRadiusPx{16};                 // Screen-pixel pick radius (default 16 px)
```

---

### Public Mouse API

Declared in `src/wx_bgi_ext.h`, implemented in `src/bgi_modern_api.cpp`.

#### `wxbgi_get_mouse_pos(int *x, int *y)` — lines 120–125

```cpp
BGI_API void BGI_CALL wxbgi_get_mouse_pos(int *x, int *y);
```

Writes the current cursor position into `*x` and `*y`. Either pointer may be NULL (only the non-NULL one is written). Acquires `gMutex`.

#### `wxbgi_mouse_moved()` — lines 127–133

```cpp
BGI_API int BGI_CALL wxbgi_mouse_moved(void);
```

Returns 1 if the cursor has moved since the last call to this function, 0 otherwise. Atomically reads and clears `gState.mouseMoved`. Acquires `gMutex`.

**Typical usage in an animation loop:**
```cpp
if (wxbgi_mouse_moved()) {
    int mx, my;
    wxbgi_get_mouse_pos(&mx, &my);
    // redraw hover highlight at (mx, my)
}
```

---

### Click-to-Pick Pipeline

Left mouse button clicks drive the DDS object selection system. The full pipeline is:

```
mouseButtonCallback(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, mods)
    |
    v
overlayPerformPick(gState.mouseX, gState.mouseY, ctrl_held)
    |
    +-- for each camera with selCursorOverlay.enabled:
    |       check click inside camera viewport
    |       for each DDS object:
    |           screenDistToObject() -> screen-space distance
    |           if distance <= selectionPickRadiusPx: add to candidates
    |       sort candidates by depth (nearest first), then screen distance
    |       select nearest candidate
    |
    v
Update gState.selectedObjectIds:
    Single-click   -> clear, then add if not already selected (re-click = deselect)
    Ctrl+click     -> toggle: add if absent, erase if present
```

The pick function (`src/bgi_overlay.cpp`, function `overlayPerformPick()`) handles all four DDS geometry types: 2D world lines/circles/rectangles, 3D solids, and 3D surfaces. Depth ordering uses the camera view matrix to project candidate object centroids and sort by Z-depth.

**Deselect behaviour:** A single click on an already-selected object deselects it (the ID is not re-added after the clear). CTRL+click also toggles deselection. This was fixed in a prior session where `selectedObjectIds.clear()` was incorrectly running before the `wasSelected` check.

Flash feedback for selected objects is driven by `gState.solidColorOverride` and the per-frame overlay rendering pass — see **VisualAids.md** for details.

---

## Event Pump

The event pump is the mechanism that causes GLFW to actually deliver accumulated OS events to the registered callbacks. It **must** be called on the **main thread**.

### `wxbgi_poll_events()` (`src/bgi_modern_api.cpp`, lines 62–73)

```cpp
BGI_API int BGI_CALL wxbgi_poll_events(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
        return -1;

    glfwPollEvents();   // <-- all callbacks fire here, synchronously
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}
```

- Acquires `gMutex` first.
- Calls `glfwPollEvents()` while holding the lock.
- All four GLFW callbacks execute **synchronously inside** this call, which means they run with `gMutex` already held — they must not attempt to re-acquire it.

### Other call sites of `glfwPollEvents()`

`glfwPollEvents()` is also called inside `wxbgi_begin_advanced_frame()` (line 349 of `bgi_modern_api.cpp`), which is used in custom render loops that want tight frame-rate control with automatic event processing.

### What happens without calling `wxbgi_poll_events()`

If the programmer's main loop never calls any event pump function:
- OS events accumulate in the system queue but never reach the callbacks.
- `keyQueue` stays empty — `wxbgi_read_key()` always returns -1.
- `mouseX`/`mouseY` never update — `wxbgi_get_mouse_pos()` returns stale values.
- The window's close button is ignored — `wxbgi_should_close()` never returns 1.
- On some platforms (macOS, Wayland) the window may appear unresponsive or frozen to the OS.

### Recommended main loop pattern

```cpp
initwindow(800, 600, "My App");

while (!wxbgi_should_close()) {
    wxbgi_poll_events();     // process all pending keyboard + mouse events

    // Read keyboard
    if (wxbgi_key_pressed()) {
        int k = wxbgi_read_key();
        if (k == 27) break;  // Escape
    }

    // Read mouse
    if (wxbgi_mouse_moved()) {
        int mx, my;
        wxbgi_get_mouse_pos(&mx, &my);
    }

    // Draw ...
    cleardevice();
    // ... drawing calls ...

    wxbgi_swap_window_buffers();
}

closegraph();
```

---

## Thread Safety

### Global Mutex

```cpp
// src/bgi_state.h
extern std::mutex gMutex;   // non-recursive std::mutex
```

`gMutex` guards all access to `bgi::gState`, including all keyboard and mouse fields.

### Mutex Acquisition Rules

| Location | Acquires gMutex? | Reason |
|---|---|---|
| `wxbgi_poll_events()` | YES — `std::lock_guard` | Public API entry point |
| `wxbgi_key_pressed()` | YES — `std::lock_guard` | Public API |
| `wxbgi_read_key()` | YES — `std::lock_guard` | Public API |
| `wxbgi_is_key_down()` | YES — `std::lock_guard` | Public API |
| `wxbgi_get_mouse_pos()` | YES — `std::lock_guard` | Public API |
| `wxbgi_mouse_moved()` | YES — `std::lock_guard` | Public API |
| `keyCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `charCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `cursorPosCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `mouseButtonCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `overlayPerformPick()` | **NO** | Called from `mouseButtonCallback` inside locked context |

### Why Callbacks Must Not Lock

`std::mutex` is **non-recursive** in C++. If `mouseButtonCallback()` (called from inside `glfwPollEvents()` which holds the lock) attempted to acquire `gMutex` again on the same thread, the behaviour is undefined — on MSVC debug builds it calls `abort()` immediately. This rule is documented explicitly in the source:

```cpp
// src/bgi_api.cpp, lines 168-172
// NOTE: Do NOT acquire gMutex here.  All GLFW callbacks fire
// synchronously on the thread that called glfwPollEvents(), which
// already holds gMutex (via flushToScreen / wxbgi_poll_events).
// Re-acquiring a non-recursive std::mutex on the same thread calls
// abort() in MSVC debug builds.
```

---

## Code Map

### Data Structures

```
src/bgi_types.h — struct BgiState
    |
    +-- keyQueue          std::queue<int>           Translated key codes (0-255 FIFO)
    +-- keyDown           std::array<uint8_t, 512>  Per-GLFW-key press state
    +-- mouseX            int                        Cursor X in window pixels
    +-- mouseY            int                        Cursor Y in window pixels
    +-- mouseMoved        bool                       True after cursor moves; cleared by API
    +-- selectedObjectIds std::vector<string>        IDs of selected DDS objects
    +-- selectionPickRadiusPx int                   Pick hit radius in screen pixels (def 16)
    +-- selectionFlashScheme  int                   Flash colour: 0=orange, 1=purple
    +-- solidColorOverride    int                   >= 0 overrides face/edge colour for flash

src/bgi_state.h
    +-- gMutex            std::mutex                 Global non-recursive state lock
    +-- gState            BgiState                   Singleton state object
```

### Keyboard Layer

```
GLFW OS event
    |
    v
keyCallback()                      src/bgi_api.cpp : 36-136
    |   Fires: key press, repeat, release
    |   Reads:  GLFW key constant, action, mods
    |   Writes: gState.keyDown[key]  (always, any action)
    |           gState.keyQueue      (press/repeat only, special keys)
    |
    +-- queueKeyCode(int)           src/bgi_api.cpp : 25-28   pushes 1 int
    +-- queueExtendedKey(int)       src/bgi_api.cpp : 30-34   pushes {0, scancode}
    |
    v
charCallback()                     src/bgi_api.cpp : 138-152
    |   Fires: printable Unicode input
    |   Reads:  codepoint (uint) from OS keyboard layout
    |   Writes: gState.keyQueue  (1-255 only; drops >255, drops 9/13/27)
    |
    +-- queueKeyCode(int)           src/bgi_api.cpp : 25-28
    |
    v
gState.keyQueue   std::queue<int>
gState.keyDown[]  std::array<uint8_t, 512>
    |
    v
Public API (src/bgi_modern_api.cpp, all acquire gMutex)
    |
    +-- wxbgi_key_pressed()        : 75-85    check keyQueue.empty()
    +-- wxbgi_read_key()           : 87-105   dequeue front, return -1 if empty
    +-- wxbgi_is_key_down(key)     : 107-118  query keyDown[key], bypass queue
    |
    +-- wxbgi_test_clear_key_queue()          : 136-150  (#ifdef TEST_SEAMS)
    +-- wxbgi_test_inject_key_code(int)       : 152-164  (#ifdef TEST_SEAMS)
    +-- wxbgi_test_inject_extended_scan(int)  : 166-179  (#ifdef TEST_SEAMS)
```

### Mouse Layer

```
GLFW OS event
    |
    v
cursorPosCallback()                src/bgi_api.cpp : 154-160
    |   Fires: cursor position change (pixels, top-left origin)
    |   Writes: gState.mouseX, gState.mouseY, gState.mouseMoved = true
    |
    v
mouseButtonCallback()              src/bgi_api.cpp : 162-175
    |   Fires: left button press
    |   Reads:  gState.mouseX, gState.mouseY, GLFW_MOD_CONTROL
    |   Calls:  overlayPerformPick(mx, my, ctrl)
    |
    v
overlayPerformPick()               src/bgi_overlay.cpp : ~668-756
    |   For each camera with selCursorOverlay.enabled:
    |       Check click inside camera viewport
    |       screenDistToObject()   src/bgi_overlay.cpp  -- screen-space distance
    |       Sort candidates by depth + dist
    |   Writes: gState.selectedObjectIds  (add / toggle / clear-and-add)
    |
    v
Public API (src/bgi_modern_api.cpp, all acquire gMutex)
    |
    +-- wxbgi_get_mouse_pos(x, y)  : 120-125  read mouseX/mouseY
    +-- wxbgi_mouse_moved()        : 127-133  read + clear mouseMoved flag
```

### Event Pump Layer

```
Programmer main loop
    |
    v
wxbgi_poll_events()                src/bgi_modern_api.cpp : 62-73
    |   Acquires: gMutex
    |   Calls:    glfwPollEvents()    <-- GLFW API; fires all callbacks here
    |   Returns:  0 on success, -1 if window not ready
    |
    v
wxbgi_begin_advanced_frame()       src/bgi_modern_api.cpp : ~349
    |   Also calls glfwPollEvents() (inside locked section)
    |   Used by: custom frame-rate-controlled render loops
    |
    v
Callback chain (no mutex inside callbacks):
    keyCallback / charCallback / cursorPosCallback / mouseButtonCallback
```

### File Dependency Diagram

```
wx_bgi_ext.h                   <-- public keyboard + mouse API declarations
    |
    v
bgi_modern_api.cpp             <-- implements wxbgi_key_pressed, wxbgi_read_key,
    |                              wxbgi_is_key_down, wxbgi_get_mouse_pos,
    |                              wxbgi_mouse_moved, wxbgi_poll_events,
    |                              wxbgi_test_inject_* (TEST_SEAMS)
    +-- bgi_state.h            <-- gMutex, gState (BgiState singleton)
    +-- bgi_draw.h             <-- bgi::flushToScreen()
    +-- GLFW/glfw3.h           <-- glfwPollEvents()

wx_bgi.h                       <-- (classic BGI API; kbhit/getch NOT implemented)

bgi_api.cpp                    <-- GLFW callback implementations + initializeWindow()
    |
    +-- bgi_state.h            <-- gState, gMutex
    +-- bgi_overlay.h          <-- overlayPerformPick()
    +-- GLFW/glfw3.h           <-- glfwSet*Callback(), GLFW_KEY_*, GLFW_MOD_*

bgi_overlay.cpp                <-- overlayPerformPick(), screenDistToObject()
    |
    +-- bgi_types.h            <-- BgiState (selectedObjectIds, mouseX/Y, etc.)
    +-- bgi_state.h            <-- gState, gMutex
    +-- bgi_camera.h           <-- camera view/proj matrix math (GLM)
    +-- bgi_dds.h              <-- DDS object iteration for pick candidates

bgi_types.h                    <-- BgiState struct (keyQueue, keyDown, mouseX/Y, etc.)

bgi_state.h / bgi_state.cpp    <-- gState singleton, gMutex definition,
                                   resetStateForWindow() (clears keyboard+mouse state)
```

---

*See also:*
- **[VisualAids.md](./VisualAids.md)** — selection flash feedback, overlay pick system, selection API reference
- **[Camera3D_Map.md](./Camera3D_Map.md)** — camera viewport math used by the click-to-pick pipeline
- **`src/wx_bgi_ext.h`** — complete public declarations with Doxygen doc-comments
- **`examples/cpp/wxbgi_keyboard_queue.cpp`** — live example of the keyboard queue API
