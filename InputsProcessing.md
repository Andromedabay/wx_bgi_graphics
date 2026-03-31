# Input Processing

This document explains how wx_BGI_Graphics captures, translates, queues, and exposes keyboard
and mouse input to the programmer. It covers the low-level GLFW backend APIs, the data structures
involved, exactly what the library's built-in callbacks do before any user hook fires, the user
hook (callback chaining) system, the threading model, and a full code map of every relevant
function.

## Table of Contents

1. [Overview](#overview)
2. [Low-Level Backend APIs](#low-level-backend-apis)
3. [Keyboard Handling](#keyboard-handling)
   - [Built-in Keyboard Callbacks](#built-in-keyboard-callbacks)
   - [Key Translation and Queue](#key-translation-and-queue)
   - [DOS-Style Extended Key Codes](#dos-style-extended-key-codes)
   - [BgiState Keyboard Fields](#bgistate-keyboard-fields)
   - [Public Keyboard API](#public-keyboard-api)
   - [Test Seams](#test-seams)
4. [Mouse Handling](#mouse-handling)
   - [Built-in Mouse Callbacks](#built-in-mouse-callbacks)
   - [Coordinate System](#coordinate-system)
   - [BgiState Mouse Fields](#bgistate-mouse-fields)
   - [Public Mouse API](#public-mouse-api)
   - [Click-to-Pick Pipeline](#click-to-pick-pipeline)
5. [Event Pump](#event-pump)
6. [Thread Safety](#thread-safety)
7. [User Input Hooks (Callback Chaining)](#user-input-hooks-callback-chaining)
   - [Design: Additive Chain](#design-additive-chain)
   - [Built-in Callback Actions Reference](#built-in-callback-actions-reference)
   - [Thread-Safety Warning](#thread-safety-warning)
   - [Hook Types and Constants](#hook-types-and-constants)
   - [Registration API](#registration-api)
   - [Usage Examples](#usage-examples)
8. [Mouse Scroll (Wheel) Events](#mouse-scroll-wheel-events)
9. [Default Behavior Bypass](#default-behavior-bypass)
10. [Hook-Context DDS Functions (`wxbgi_hk_*`)](#hook-context-dds-functions-wxbgi_hk_)
11. [Test and Example Programs](#test-and-example-programs)
12. [Code Map](#code-map)
    - [Data Structures](#data-structures)
    - [Keyboard Layer](#keyboard-layer)
    - [Mouse Layer](#mouse-layer)
    - [Event Pump Layer](#event-pump-layer)
    - [File Dependency Diagram](#file-dependency-diagram)

---

## Overview

Input handling in wx_BGI_Graphics is a **synchronous, single-threaded, callback-driven pipeline**
built on top of GLFW. The programmer drives the input loop by calling `wxbgi_poll_events()` (or
the frame helpers) from their main loop. GLFW then fires any registered callbacks synchronously on
that same thread. Keyboard events are buffered in an in-memory queue; mouse position is tracked in
plain integer fields; mouse button clicks directly trigger the object pick/selection system.

After each built-in callback completes its own internal processing, an optional **user hook** is
called if one has been registered. This gives library users a direct, low-latency path to all input
events without losing any of the library's built-in functionality.

```
Programmer main loop
    |
    v
wxbgi_poll_events()   <-- acquires gMutex, calls glfwPollEvents()
    |
    +-- [key event]    --> keyCallback()         --> keyDown[] updated, keyQueue pushed
    |                                            --> userKeyHook() called if set
    +-- [char event]   --> charCallback()        --> keyQueue pushed
    |                                            --> userCharHook() called if set
    +-- [cursor move]  --> cursorPosCallback()   --> mouseX/Y updated, mouseMoved=true
    |                                            --> userCursorPosHook() called if set
    +-- [mouse click]  --> mouseButtonCallback() --> overlayPerformPick() (left only)
                                                 --> selectedObjectIds updated
                                                 --> userMouseButtonHook() called if set
```

---

## Low-Level Backend APIs

The library relies exclusively on **GLFW 3.4** for window management and raw input events. No
platform-specific input APIs (Win32 `GetAsyncKeyState`, X11 `XNextEvent`, Cocoa `NSEvent`) are
used directly — GLFW abstracts them.

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

> **Important:** GLFW supports only **one** registered callback per event type per window.
> The library owns all four callbacks. Users must **not** call `glfwSet*Callback()` directly —
> doing so would silently destroy the key queue, keyDown state, mouse tracking, and selection
> system. Use the `wxbgi_set_*_hook()` API instead (see [User Input Hooks](#user-input-hooks-callback-chaining)).

---

## Keyboard Handling

### Built-in Keyboard Callbacks

Two GLFW callbacks cooperate to cover the full range of keyboard input.

#### `keyCallback` — special and control keys (`src/bgi_api.cpp`)

Fires on **every** key press, repeat, and release.

**Step-by-step actions performed by the library:**

1. **`keyDown[]` update (always, for all actions including release):**
   Sets `gState.keyDown[key] = (action != GLFW_RELEASE) ? 1 : 0`.
   This keeps the instantaneous raw-key state table current for `wxbgi_is_key_down()`.

2. **Key queue push (press and repeat actions only):**
   Translates special/control keys into DOS-compatible byte sequences and pushes them
   into `gState.keyQueue`:
   - Control characters: Escape → 27, Enter → 13, Tab → 9, Backspace → 8 (single byte each)
   - Navigation/function keys → two-byte `{0, scancode}` sequence (see [DOS-Style Extended Key Codes](#dos-style-extended-key-codes))
   - Printable characters are **not** pushed here — delegated to `charCallback` (avoids double-queuing)

3. **`userKeyHook` invocation (after all library logic, all actions):**
   If `gState.userKeyHook != nullptr`, it is called with `(key, scancode, action, mods)`.
   By this point `keyDown[key]` reflects the new state and any key code has already been enqueued.

**State guaranteed to be current when `WxbgiKeyHook` fires:**
- `gState.keyDown[key]` — already updated to the new press/release state
- `gState.keyQueue` — already has the translated code pushed (for press/repeat of special keys)

#### `charCallback` — printable character input (`src/bgi_api.cpp`)

Fires only for printable Unicode input (letter, digit, symbol with any modifier).
GLFW fires this **in addition to** `keyCallback` for printable keys, giving the OS-composed
character (correct layout, Shift, dead keys, IME).

**Step-by-step actions performed by the library:**

1. **Filtering:** Drops codepoints ≤ 0 or > 255 (non-Latin characters unsupported in classic BGI).
   Also drops Tab (9), Enter (13), Escape (27) — those already arrived via `keyCallback`.

2. **Key queue push:** Pushes the accepted codepoint as-is into `gState.keyQueue`.

3. **`userCharHook` invocation:** If `gState.userCharHook != nullptr`, called with `(codepoint)`.
   The code is already in `keyQueue` by this point.

**State guaranteed to be current when `WxbgiCharHook` fires:**
- `gState.keyQueue` — already contains the just-pushed codepoint

**Why two callbacks?** GLFW guarantees that `charCallback` produces the correct OS-composed
character (honouring keyboard layout, Shift, dead keys). Using it for printable characters and
`keyCallback` for control/special keys avoids duplication and correctly handles international layouts.

---

### Key Translation and Queue

#### Queue helpers (`src/bgi_api.cpp`, anonymous namespace)

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

`queueExtendedKey()` produces the classic two-byte sequence `{0, scancode}` that DOS programs
expect for non-printable keys.

#### Storage

```cpp
// src/bgi_types.h
std::queue<int> keyQueue;                    // Unbounded FIFO of translated codes (0-255)
std::array<std::uint8_t, 512> keyDown{};    // keyDown[GLFW_KEY_*] = 1 if held, 0 if released
```

---

### DOS-Style Extended Key Codes

Classic Borland BGI programs read the keyboard with `getch()`, which returns 0 for extended keys
and then returns the scancode on the next call. This library emulates that behaviour exactly by
pushing **two** integers for each special key.

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
WxbgiKeyHook                  userKeyHook{nullptr};   // User hook (NULL = disabled)
WxbgiCharHook                 userCharHook{nullptr};  // User hook (NULL = disabled)
```

---

### Public Keyboard API

All public keyboard functions are declared in `src/wx_bgi_ext.h` and implemented in
`src/bgi_modern_api.cpp`. Each acquires `gMutex` via `std::lock_guard`.

#### `wxbgi_key_pressed()`

```cpp
BGI_API int BGI_CALL wxbgi_key_pressed(void);
```

Returns 1 if at least one key code is waiting in `keyQueue`, 0 if empty. Does not consume the
queued code. Equivalent to the DOS `kbhit()` function.

**Usage pattern:**
```cpp
while (wxbgi_key_pressed()) {
    int k = wxbgi_read_key();
    // handle k
}
```

#### `wxbgi_read_key()`

```cpp
BGI_API int BGI_CALL wxbgi_read_key(void);
```

Dequeues and returns the next translated key code (0–255), or -1 if the queue is empty. For
extended keys, the first call returns 0 and the next call returns the scancode.

**Usage pattern:**
```cpp
int k = wxbgi_read_key();
if (k == 0) {
    int scan = wxbgi_read_key();  // e.g. 72 = Up arrow
}
```

#### `wxbgi_is_key_down(int key)`

```cpp
BGI_API int BGI_CALL wxbgi_is_key_down(int key);
```

Queries the **instantaneous** press state of a key by its GLFW constant. Returns 1 if currently
held, 0 if released, -1 if `key` is out of range. Bypasses the queue — useful for detecting held
modifier or navigation keys without consuming queue entries.

**Usage pattern:**
```cpp
if (wxbgi_is_key_down(GLFW_KEY_LEFT_SHIFT)) {
    // Shift is currently held
}
```

---

### Test Seams

When compiled with `WXBGI_ENABLE_TEST_SEAMS`, three additional functions are available for CI
keyboard injection:

| Function | Purpose |
|---|---|
| `wxbgi_test_clear_key_queue()` | Empties `keyQueue` |
| `wxbgi_test_inject_key_code(int)` | Pushes one code (0–255) into `keyQueue` |
| `wxbgi_test_inject_extended_scan(int)` | Pushes `{0, scanCode}` into `keyQueue` |

These functions acquire `gMutex` and directly manipulate `gState.keyQueue`, simulating what the
GLFW callbacks would do during real key presses.

---

## Mouse Handling

### Built-in Mouse Callbacks

#### `cursorPosCallback` (`src/bgi_api.cpp`)

Fires on every mouse movement inside the window.

**Step-by-step actions performed by the library:**

1. **Position update:** Converts GLFW's `double xpos, ypos` to `int` and writes into
   `gState.mouseX` and `gState.mouseY`. Origin is top-left (0,0); matches BGI pixel space.

2. **Movement flag:** Sets `gState.mouseMoved = true`. Cleared by `wxbgi_mouse_moved()`.

3. **`userCursorPosHook` invocation:** If non-null, called with `(int x, int y)` —
   the same values already written to `gState.mouseX/Y`.

**State guaranteed to be current when `WxbgiCursorPosHook` fires:**
- `gState.mouseX`, `gState.mouseY` — already updated to the new position
- `gState.mouseMoved` — already set to `true`

#### `mouseButtonCallback` (`src/bgi_api.cpp`)

Fires on any mouse button press or release.

**Step-by-step actions performed by the library:**

1. **Pick/selection (left button press only):**
   When `button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS`:
   - Detects CTRL modifier (`mods & GLFW_MOD_CONTROL`)
   - Calls `overlayPerformPick(gState.mouseX, gState.mouseY, ctrl)` which:
     - Iterates all cameras with `selCursorOverlay.enabled`
     - For each camera, projects all DDS objects to screen space and finds the nearest
       within `selectionPickRadiusPx` pixels
     - Updates `gState.selectedObjectIds` (single-click: clear+add or deselect;
       CTRL+click: toggle)
   - Right button, middle button, and release events: **no** library action, just pass to hook

2. **`userMouseButtonHook` invocation (all buttons, all actions):**
   If non-null, called with `(button, action, mods)`.

**State guaranteed to be current when `WxbgiMouseButtonHook` fires:**
- `gState.mouseX`, `gState.mouseY` — already current (from last `cursorPosCallback`)
- `gState.selectedObjectIds` — already updated by pick logic (for left-press events)
- Your hook fires **after** selection is resolved, so you can immediately act on the new selection

> **Scroll wheel:** `glfwSetScrollCallback` is **not** registered. Scroll events are not captured
> in the current implementation.

---

### Coordinate System

GLFW delivers mouse positions in **window-pixel space** with the origin at the **top-left corner**:

```
(0,0) -----> +X (right)
  |
  v
+Y (down)
```

These coordinates match the BGI pixel coordinate system directly — no transformation is applied.
`gState.mouseX/Y` are ready to use for BGI-pixel-space hit tests.

---

### BgiState Mouse Fields

Defined in `src/bgi_types.h`, struct `BgiState`:

```cpp
int  mouseX{0};          // Current cursor X in window pixels (top-left = 0,0)
int  mouseY{0};          // Current cursor Y in window pixels
bool mouseMoved{false};  // True after any cursor move; cleared by wxbgi_mouse_moved()

// Selection state (updated by click-to-pick before mouse button hook fires)
std::vector<std::string> selectedObjectIds;    // IDs of currently selected DDS objects
int selectionFlashScheme{0};                   // 0 = orange flash, 1 = purple flash
int selectionPickRadiusPx{16};                 // Screen-pixel pick radius (default 16 px)

// User hooks
WxbgiCursorPosHook   userCursorPosHook{nullptr};   // NULL = disabled
WxbgiMouseButtonHook userMouseButtonHook{nullptr}; // NULL = disabled
```

---

### Public Mouse API

Declared in `src/wx_bgi_ext.h`, implemented in `src/bgi_modern_api.cpp`.

#### `wxbgi_get_mouse_pos(int *x, int *y)`

```cpp
BGI_API void BGI_CALL wxbgi_get_mouse_pos(int *x, int *y);
```

Writes the current cursor position into `*x` and `*y`. Either pointer may be NULL. Acquires `gMutex`.

#### `wxbgi_mouse_moved()`

```cpp
BGI_API int BGI_CALL wxbgi_mouse_moved(void);
```

Returns 1 if the cursor has moved since the last call to this function, 0 otherwise. Atomically
reads and clears `gState.mouseMoved`. Acquires `gMutex`.

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

Left mouse button clicks drive the DDS object selection system. This pipeline runs inside
`mouseButtonCallback` **before** the user's `WxbgiMouseButtonHook` fires:

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
    |
    v
userMouseButtonHook() called  <-- selection result already in gState.selectedObjectIds
```

`overlayPerformPick()` (`src/bgi_overlay.cpp`) handles all DDS geometry types: 2D world
lines/circles/rectangles, 3D solids, and 3D surfaces. Depth ordering uses the camera view matrix
to project candidate centroids and sort by Z-depth.

Flash feedback for selected objects is driven by `gState.solidColorOverride` — see
**[VisualAids.md](./VisualAids.md)** for details.

---

## Event Pump

The event pump causes GLFW to deliver accumulated OS events to the registered callbacks. It
**must** be called on the **main thread**.

### `wxbgi_poll_events()` (`src/bgi_modern_api.cpp`)

```cpp
BGI_API int BGI_CALL wxbgi_poll_events(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!ensureReadyUnlocked())
        return -1;

    glfwPollEvents();   // <-- all callbacks (and user hooks) fire here, synchronously
    bgi::gState.lastResult = bgi::grOk;
    return 0;
}
```

- Acquires `gMutex` first.
- Calls `glfwPollEvents()` while holding the lock.
- All four GLFW callbacks (and any registered user hooks) execute **synchronously inside**
  this call with `gMutex` already held.

### Other call sites of `glfwPollEvents()`

`glfwPollEvents()` is also called inside `wxbgi_begin_advanced_frame()`, which is used in
custom render loops that want tight frame-rate control with automatic event processing.

### What happens without calling `wxbgi_poll_events()`

- OS events accumulate in the system queue but never reach the callbacks or user hooks.
- `keyQueue` stays empty — `wxbgi_read_key()` always returns -1.
- `mouseX`/`mouseY` never update — `wxbgi_get_mouse_pos()` returns stale values.
- The window's close button is ignored — `wxbgi_should_close()` never returns 1.
- User hooks never fire.
- On some platforms (macOS, Wayland) the window may appear frozen to the OS.

### Recommended main loop pattern

```cpp
initwindow(800, 600, "My App");

while (!wxbgi_should_close()) {
    wxbgi_poll_events();     // process all pending keyboard + mouse events + fire user hooks

    if (wxbgi_key_pressed()) {
        int k = wxbgi_read_key();
        if (k == 27) break;  // Escape
    }

    if (wxbgi_mouse_moved()) {
        int mx, my;
        wxbgi_get_mouse_pos(&mx, &my);
    }

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

`gMutex` guards all access to `bgi::gState`, including all keyboard, mouse, and hook fields.

### Mutex Acquisition Rules

| Location | Acquires gMutex? | Reason |
|---|---|---|
| `wxbgi_poll_events()` | YES — `std::lock_guard` | Public API entry point |
| `wxbgi_key_pressed()` | YES — `std::lock_guard` | Public API |
| `wxbgi_read_key()` | YES — `std::lock_guard` | Public API |
| `wxbgi_is_key_down()` | YES — `std::lock_guard` | Public API |
| `wxbgi_get_mouse_pos()` | YES — `std::lock_guard` | Public API |
| `wxbgi_mouse_moved()` | YES — `std::lock_guard` | Public API |
| `wxbgi_set_key_hook()` | YES — `std::lock_guard` | Public API |
| `wxbgi_set_char_hook()` | YES — `std::lock_guard` | Public API |
| `wxbgi_set_cursor_pos_hook()` | YES — `std::lock_guard` | Public API |
| `wxbgi_set_mouse_button_hook()` | YES — `std::lock_guard` | Public API |
| `keyCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `charCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `cursorPosCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `mouseButtonCallback()` | **NO** | Called while lock held by `glfwPollEvents()` caller |
| `overlayPerformPick()` | **NO** | Called from `mouseButtonCallback` inside locked context |
| **User hooks** (`userKeyHook` etc.) | **NO** | Called from within GLFW callbacks — mutex already held |

### Why Callbacks Must Not Lock

`std::mutex` is **non-recursive** in C++. If any callback or user hook attempted to acquire
`gMutex` again on the same thread (already locked by `wxbgi_poll_events()`), the behaviour is
undefined — on MSVC debug builds it calls `abort()` immediately. This rule is documented in the
source at `src/bgi_api.cpp`.

---

## User Input Hooks (Callback Chaining)

The library provides an optional hook API that lets programs in any language receive keyboard and
mouse events directly, alongside the library's own internal processing. This is the correct way
to extend input handling — never call `glfwSet*Callback()` directly.

### Design: Additive Chain

The library retains full ownership of all 4 GLFW callbacks. Hooks are **additive**:
the library's own logic always runs first, then the user hook is called if registered.

```
GLFW event arrives
    |
    v
Step 1: Library internal processing (see "Built-in Callback Actions Reference" below)
    |       keyDown[], keyQueue, mouseX/Y, overlayPerformPick() etc.
    |
    v
Step 2: User hook called if non-NULL (additive, cannot suppress library processing)
    |       gState already reflects all state changes from Step 1
    v
Returns from callback
```

Pass `NULL` to any registration function to remove a previously installed hook.

---

### Built-in Callback Actions Reference

This table summarises exactly what the library has done by the time your hook fires.
Use it when implementing hooks to know what state is already up-to-date.

| Callback / Hook | Trigger | Library actions completed before hook fires | State ready in hook |
|---|---|---|---|
| `keyCallback` / `WxbgiKeyHook` | Any key press, repeat, or release | `keyDown[key]` updated to new state; for press/repeat of special/control keys: translated code(s) pushed to `keyQueue` | `gState.keyDown[key]` is current; `gState.keyQueue` has any new codes |
| `charCallback` / `WxbgiCharHook` | Printable character typed | Codepoint (1-255) pushed to `keyQueue`; out-of-range and Tab/Enter/Esc dropped | `gState.keyQueue` front is the just-pushed codepoint |
| `cursorPosCallback` / `WxbgiCursorPosHook` | Mouse cursor moved | `mouseX` and `mouseY` set to new position (int pixels); `mouseMoved = true` | `gState.mouseX`, `gState.mouseY` are current new position |
| `mouseButtonCallback` / `WxbgiMouseButtonHook` | Any mouse button pressed or released | **Left press only:** `overlayPerformPick()` ran and `selectedObjectIds` updated (add/toggle/deselect). Right/middle/release: no library action | `gState.selectedObjectIds` reflects latest selection; `gState.mouseX/Y` are current |

**Additional notes:**
- `keyCallback` fires for **all** keys on press, repeat, and release. `charCallback` fires
  only for printable keys on press (and repeat if OS sends char repeats). For a plain letter
  like 'A', both `keyCallback` (GLFW key code 65) AND `charCallback` (codepoint 65 or 97
  depending on shift) will fire — the char codepoint is what goes into `keyQueue`.
- The key hook receives the raw **GLFW key code** (layout-independent); the char hook receives
  the OS-composed **Unicode codepoint** (layout-dependent). Use the char hook for text input;
  use the key hook for keyboard shortcuts.
- For right/middle mouse clicks and mouse button releases, only the user hook fires — the
  library performs no internal action for those events. This makes it easy to add right-click
  context menus or drag detection without any conflict.

---

### Thread-Safety Warning

> **CRITICAL:** User hooks fire from within a GLFW callback, which executes synchronously
> inside `glfwPollEvents()` **while `gMutex` is held**.
> Hooks **must not** call any `wxbgi_*` function — doing so deadlocks on the non-recursive
> `std::mutex` (and calls `abort()` in MSVC debug builds).
>
> **Safe from a hook:** update your own variables, set flags, call non-wxbgi code,
> enqueue to your own data structures, call OS/platform APIs that don't re-enter the library.
>
> **Unsafe from a hook:** any `wxbgi_*` call, including `wxbgi_get_mouse_pos()`,
> `wxbgi_read_key()`, `wxbgi_poll_events()`, etc.

Read mouse/keyboard state **after** `wxbgi_poll_events()` returns using the normal public API:

```cpp
// WRONG: calling wxbgi_* inside a hook
static void BGI_CALL myHook(int key, int scancode, int action, int mods) {
    int x, y;
    wxbgi_get_mouse_pos(&x, &y);  // DEADLOCK!
}

// CORRECT: use state passed in hook parameters, or read via public API after poll_events
static int gLastKey = 0;
static void BGI_CALL myHook(int key, int scancode, int action, int mods) {
    gLastKey = key;  // just set a flag; no wxbgi_* calls
}
// ... then later in main loop after wxbgi_poll_events() returns:
if (gLastKey == 256) { /* Escape */ }
```

---

### Hook Types and Constants

The following typedefs are defined in `src/bgi_types.h` at global scope, available to all
languages without requiring GLFW headers:

```c
// Key press / repeat / release hook
typedef void (BGI_CALL *WxbgiKeyHook)(int key, int scancode, int action, int mods);

// Printable Unicode character hook (layout-composed codepoint)
typedef void (BGI_CALL *WxbgiCharHook)(unsigned int codepoint);

// Mouse cursor movement hook (integer pixels, top-left origin)
typedef void (BGI_CALL *WxbgiCursorPosHook)(int x, int y);

// Mouse button press / release hook (all buttons, all actions)
typedef void (BGI_CALL *WxbgiMouseButtonHook)(int button, int action, int mods);
```

`BGI_CALL` is `__cdecl` on MSVC Windows and empty (default C calling convention) elsewhere.

#### Action, button, and modifier constants

Defined in `src/wx_bgi_ext.h`; mirror GLFW values exactly:

| Constant | Value | Meaning |
|---|---|---|
| `WXBGI_KEY_PRESS` | 1 | Key or button pressed |
| `WXBGI_KEY_RELEASE` | 0 | Key or button released |
| `WXBGI_KEY_REPEAT` | 2 | Key held (auto-repeat) |
| `WXBGI_MOUSE_LEFT` | 0 | Left mouse button |
| `WXBGI_MOUSE_RIGHT` | 1 | Right mouse button |
| `WXBGI_MOUSE_MIDDLE` | 2 | Middle mouse button |
| `WXBGI_MOD_SHIFT` | 0x0001 | Shift modifier held |
| `WXBGI_MOD_CTRL` | 0x0002 | Control modifier held |
| `WXBGI_MOD_ALT` | 0x0004 | Alt modifier held |

---

### Registration API

All four functions are declared in `src/wx_bgi_ext.h` and implemented in `src/bgi_modern_api.cpp`.
Each acquires `gMutex` briefly to store the pointer, then returns immediately.

```c
BGI_API void BGI_CALL wxbgi_set_key_hook(WxbgiKeyHook cb);
BGI_API void BGI_CALL wxbgi_set_char_hook(WxbgiCharHook cb);
BGI_API void BGI_CALL wxbgi_set_cursor_pos_hook(WxbgiCursorPosHook cb);
BGI_API void BGI_CALL wxbgi_set_mouse_button_hook(WxbgiMouseButtonHook cb);
```

| Function | Hook fires when | Parameters |
|---|---|---|
| `wxbgi_set_key_hook` | Any key press, repeat, or release | `key` (GLFW key code), `scancode` (OS scancode), `action` (`WXBGI_KEY_*`), `mods` (bitfield) |
| `wxbgi_set_char_hook` | Printable character typed (1-255) | `codepoint` (unsigned int, OS-composed) |
| `wxbgi_set_cursor_pos_hook` | Mouse cursor moves | `x`, `y` (window pixels, top-left origin) |
| `wxbgi_set_mouse_button_hook` | Any mouse button press or release | `button` (`WXBGI_MOUSE_*`), `action` (`WXBGI_KEY_PRESS/RELEASE`), `mods` |

---

### Usage Examples

#### C / C++

```c
#include "wx_bgi_ext.h"

static int  gCameraSpeed      = 0;
static int  gContextMenuPending = 0;

static void BGI_CALL myKeyHook(int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;
    // GLFW_KEY_W = 87, GLFW_KEY_S = 83
    // keyDown[] already updated; key code already in queue — just set application state
    if (key == 87 && action == WXBGI_KEY_PRESS)   gCameraSpeed = +1;
    if (key == 83 && action == WXBGI_KEY_PRESS)   gCameraSpeed = -1;
    if (action == WXBGI_KEY_RELEASE)               gCameraSpeed =  0;
}

static void BGI_CALL myMouseButtonHook(int button, int action, int mods)
{
    (void)mods;
    // For left click: selectedObjectIds already updated by the time we get here
    if (button == WXBGI_MOUSE_RIGHT && action == WXBGI_KEY_PRESS)
        gContextMenuPending = 1;  // handle after poll_events() returns
}

int main(void)
{
    initwindow(800, 600, "My App");
    wxbgi_set_key_hook(myKeyHook);
    wxbgi_set_mouse_button_hook(myMouseButtonHook);

    while (!wxbgi_should_close()) {
        wxbgi_poll_events();   // hooks fire here; gCameraSpeed may be updated

        if (gContextMenuPending) {
            gContextMenuPending = 0;
            int mx, my;
            wxbgi_get_mouse_pos(&mx, &my);  // safe: after poll_events returned
            // open context menu at (mx, my) ...
        }
        // use gCameraSpeed to move camera ...
    }
    closegraph();
}
```

#### FreePascal

```pascal
uses wx_bgi_ext;

var
  gLastKey: Integer = 0;

{ keyDown[] already updated and key code already queued when this fires }
procedure MyKeyHook(key, scancode, action, mods: Integer); cdecl;
begin
  if action = WXBGI_KEY_PRESS then
    gLastKey := key;
  { Do NOT call wxbgi_* here }
end;

begin
  initwindow(800, 600, 'Pascal App');
  wxbgi_set_key_hook(@MyKeyHook);
  while wxbgi_should_close() = 0 do
  begin
    wxbgi_poll_events();
    if gLastKey = 256 then  { GLFW_KEY_ESCAPE = 256 }
      wxbgi_request_close();
    gLastKey := 0;
  end;
  closegraph();
end.
```

#### Python 3 (ctypes)

```python
import ctypes
from wx_bgi import lib  # your ctypes-loaded library handle

# Match the BGI_CALL (__cdecl) convention
KeyHookType      = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int)
CharHookType     = ctypes.CFUNCTYPE(None, ctypes.c_uint)
CursorHookType   = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int)
MouseBtnHookType = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int, ctypes.c_int)

WXBGI_KEY_PRESS   = 1
WXBGI_MOUSE_RIGHT = 1

last_key = [0]

def on_key(key, scancode, action, mods):
    # keyDown[] already updated; queue already has the code
    if action == WXBGI_KEY_PRESS:
        last_key[0] = key
    # Do NOT call lib.wxbgi_* here

def on_cursor(x, y):
    # mouseX/Y already updated in gState — x,y are those same values
    pass  # safe to update own state here; no wxbgi_* calls

# Keep references alive (prevent garbage collection)
_key_cb  = KeyHookType(on_key)
_cur_cb  = CursorHookType(on_cursor)

lib.wxbgi_set_key_hook(_key_cb)
lib.wxbgi_set_cursor_pos_hook(_cur_cb)

lib.initwindow(800, 600, b"Python App")
while not lib.wxbgi_should_close():
    lib.wxbgi_poll_events()   # hooks fire here
    if last_key[0] == 256:    # Escape
        break
lib.closegraph()
```

#### Node.js (ffi-napi)

```javascript
const ffi = require('ffi-napi');

const lib = ffi.Library('wx_bgi_opengl', {
  initwindow:                  ['void', ['int', 'int', 'string']],
  wxbgi_poll_events:           ['int',  []],
  wxbgi_should_close:          ['int',  []],
  closegraph:                  ['void', []],
  wxbgi_set_key_hook:          ['void', ['pointer']],
  wxbgi_set_cursor_pos_hook:   ['void', ['pointer']],
  wxbgi_set_mouse_button_hook: ['void', ['pointer']],
});

const WXBGI_KEY_PRESS   = 1;
const WXBGI_MOUSE_RIGHT = 1;
let lastKey = 0;

// Keep callback references alive to prevent GC
const keyHook = ffi.Callback('void', ['int', 'int', 'int', 'int'],
  (key, scancode, action, mods) => {
    // keyDown[] already updated; do NOT call lib.wxbgi_* here
    if (action === WXBGI_KEY_PRESS) lastKey = key;
  });

const cursorHook = ffi.Callback('void', ['int', 'int'],
  (x, y) => { /* mouseX/Y already updated; safe to update own state */ });

const mouseHook = ffi.Callback('void', ['int', 'int', 'int'],
  (button, action, mods) => {
    // selectedObjectIds already updated for left-click picks
    if (button === WXBGI_MOUSE_RIGHT && action === WXBGI_KEY_PRESS) {
      // set own flag; handle after wxbgi_poll_events returns
    }
  });

lib.initwindow(800, 600, 'Node App');
lib.wxbgi_set_key_hook(keyHook);
lib.wxbgi_set_cursor_pos_hook(cursorHook);
lib.wxbgi_set_mouse_button_hook(mouseHook);

while (!lib.wxbgi_should_close()) {
  lib.wxbgi_poll_events();  // all hooks fire here
}
lib.closegraph();
```

---

## Mouse Scroll (Wheel) Events

The library captures scroll wheel input via GLFW's `glfwSetScrollCallback` and provides both an
accumulation API and a hook for real-time scroll reaction.

### How Scroll Works

1. GLFW fires `scrollCallback(window, xoffset, yoffset)` during `glfwPollEvents()`.
2. If `WXBGI_DEFAULT_SCROLL_ACCUM` is active, `xoffset`/`yoffset` are added to
   `gState.scrollDeltaX` / `gState.scrollDeltaY`.
3. The registered `userScrollHook` (if any) fires with the raw per-event deltas.

Call `wxbgi_get_scroll_delta()` from your main loop to atomically read and clear the
accumulated totals:

```c
double dx, dy;
wxbgi_get_scroll_delta(&dx, &dy);   // resets deltas to 0.0 after reading
if (dy != 0.0) zoom_camera(dy);
```

### Scroll Hook Type

```c
// In bgi_types.h (before namespace bgi)
typedef void (BGI_CALL *WxbgiScrollHook)(double xoffset, double yoffset);
```

### Scroll Registration API

| Function | Description |
|---|---|
| `wxbgi_set_scroll_hook(cb)` | Install (or remove with NULL) the scroll hook. |
| `wxbgi_get_scroll_delta(dx, dy)` | Read + clear accumulated scroll deltas. |

### Scroll Test Seam

```c
#ifdef WXBGI_ENABLE_TEST_SEAMS
int wxbgi_test_simulate_scroll(double xoffset, double yoffset);
#endif
```

---

## Default Behavior Bypass

By default all four built-in callback behaviors are active. You can disable any combination
selectively using `wxbgi_set_input_defaults()`.

### Why Bypass?

| Scenario | Bypass flag |
|---|---|
| Rolling your own key buffer | `WXBGI_DEFAULT_KEY_QUEUE` |
| Implementing your own mouse tracker | `WXBGI_DEFAULT_CURSOR_TRACK` |
| Replacing the auto-pick on left-click | `WXBGI_DEFAULT_MOUSE_PICK` |
| Handling scroll yourself (camera zoom etc.) | `WXBGI_DEFAULT_SCROLL_ACCUM` |

### Default Flags

| Constant | Value | Controls |
|---|---|---|
| `WXBGI_DEFAULT_KEY_QUEUE` | `0x01` | `keyCallback` queues key codes; `charCallback` queues chars |
| `WXBGI_DEFAULT_CURSOR_TRACK` | `0x02` | `cursorPosCallback` updates `mouseX`/`mouseY`/`mouseMoved` |
| `WXBGI_DEFAULT_MOUSE_PICK` | `0x04` | `mouseButtonCallback` calls `overlayPerformPick()` on left-click |
| `WXBGI_DEFAULT_SCROLL_ACCUM` | `0x08` | `scrollCallback` accumulates `scrollDeltaX/Y` |
| `WXBGI_DEFAULT_ALL` | `0x0F` | All defaults active (initial state) |
| `WXBGI_DEFAULT_NONE` | `0x00` | All defaults disabled |

> **Note:** `keyDown[]` (raw hardware state) is **always** updated regardless of bypass flags.
> User hooks always fire regardless of bypass flags.

### Bypass API

```c
void wxbgi_set_input_defaults(int flags);  // set bitmask
int  wxbgi_get_input_defaults(void);       // query current bitmask
```

### C Example — Custom Pick on Click

```c
// Disable library auto-pick; implement own selection logic in mouse hook
wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_MOUSE_PICK);
wxbgi_set_mouse_button_hook(myMouseHook);

// Inside hook (mutex is held — use wxbgi_hk_* functions only):
static void BGI_CALL myMouseHook(int btn, int act, int mods) {
    if (btn == WXBGI_MOUSE_LEFT && act == WXBGI_KEY_PRESS) {
        // Custom spatial filter before picking
        if (isInMyRegion(wxbgi_hk_get_mouse_x(), wxbgi_hk_get_mouse_y()))
            wxbgi_hk_dds_pick_at(wxbgi_hk_get_mouse_x(),
                                 wxbgi_hk_get_mouse_y(), 0);
    }
}
```

### FreePascal Example — Bypass Key Queue

```pascal
{ Disable automatic key queuing; handle keys manually in key hook }
wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL and (not WXBGI_DEFAULT_KEY_QUEUE));
wxbgi_set_key_hook(@MyKeyHook);

procedure MyKeyHook(key, scan, action, mods: Integer); cdecl;
begin
  { Build your own key state table here }
end;
```

---

## Hook-Context DDS Functions (`wxbgi_hk_*`)

All `wxbgi_*` functions acquire `gMutex` internally.  Because user hooks fire **while** the mutex
is held (inside `glfwPollEvents()`), calling any ordinary `wxbgi_*` function from a hook causes
a deadlock.

The `wxbgi_hk_*` family provides safe alternatives that skip mutex acquisition.  **Call them only
from within a registered hook callback.**

### Thread Safety Note

Outside a hook, calling `wxbgi_hk_*` directly is technically a data race in multi-threaded code.
In single-threaded programs (the common case for BGI applications) it is safe.

### Hook-Context API Reference

| Function | Description |
|---|---|
| `wxbgi_hk_get_mouse_x()` | Current cursor X (pixels). |
| `wxbgi_hk_get_mouse_y()` | Current cursor Y (pixels). |
| `wxbgi_hk_dds_get_selected_count()` | Number of currently selected DDS objects. |
| `wxbgi_hk_dds_get_selected_id(i, buf, maxLen)` | Copy ID of the i-th selected object into buf. Returns string length or -1. |
| `wxbgi_hk_dds_is_selected(id)` | Returns 1 if the object with that ID is selected. |
| `wxbgi_hk_dds_select(id)` | Add an object to the selection. |
| `wxbgi_hk_dds_deselect(id)` | Remove an object from the selection. |
| `wxbgi_hk_dds_deselect_all()` | Clear the entire selection. |
| `wxbgi_hk_dds_pick_at(x, y, ctrl)` | Run the spatial pick algorithm at (x,y). ctrl≠0 = toggle. Returns new count. |

### C++ Example — Read Selection in Scroll Hook

```cpp
static void BGI_CALL myScrollHook(double, double yoffset) {
    // Safe: mutex is held, so hk_* functions are appropriate here
    int n = wxbgi_hk_dds_get_selected_count();
    if (n > 0 && yoffset != 0.0) {
        char id[256];
        wxbgi_hk_dds_get_selected_id(0, id, sizeof(id));
        // Scale first selected object up/down
        scaleObject(id, yoffset);
    }
}
```

### Pascal Example

```pascal
procedure MyScrollHook(x, y: Double); cdecl;
var
  count: Integer;
  idbuf: array[0..255] of Char;
begin
  count := wxbgi_hk_dds_get_selected_count;
  if count > 0 then
    wxbgi_hk_dds_get_selected_id(0, @idbuf[0], SizeOf(idbuf));
end;
```

---

## Test and Example Programs

### C++ Programs

| Program | Description |
|---|---|
| `examples/cpp/test_input_hooks.cpp` | Automated: registers/deregisters all hooks; simulates key/char/cursor/mouse via test seams; 20+ assertions. |
| `examples/cpp/test_input_bypass.cpp` | Automated: scroll hook, `WXBGI_DEFAULT_*` bypass flags, `wxbgi_hk_*` DDS functions. |
| `examples/cpp/wxbgi_input_hooks.cpp` | Interactive demo: 4 status panels, arrow-key marker, char text area, mouse crosshair, click stamps. |

### FreePascal Programs

| Program | Description |
|---|---|
| `examples/demoFreePascal/test_input_hooks.pas` | Pascal mirror of `test_input_hooks.cpp`. |
| `examples/demoFreePascal/test_input_bypass.pas` | Pascal mirror of `test_input_bypass.cpp`. |
| `examples/demoFreePascal/demo_input_hooks.pas` | Pascal interactive hook demo. |

### Running Automated Tests

```bash
# C++ (requires WXBGI_ENABLE_TEST_SEAMS)
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

The test seam functions simulate the exact same callback→state→hook pipeline as real GLFW events:

| Seam function | Simulates |
|---|---|
| `wxbgi_test_simulate_key(key, scan, action, mods)` | `keyCallback` |
| `wxbgi_test_simulate_char(codepoint)` | `charCallback` |
| `wxbgi_test_simulate_cursor(x, y)` | `cursorPosCallback` (respects `WXBGI_DEFAULT_CURSOR_TRACK`) |
| `wxbgi_test_simulate_mouse_button(btn, action, mods)` | `mouseButtonCallback` (omits pick; needs framebuffer) |
| `wxbgi_test_simulate_scroll(xoffset, yoffset)` | `scrollCallback` (respects `WXBGI_DEFAULT_SCROLL_ACCUM`) |

---

## Code Map

### Data Structures

```
src/bgi_types.h — struct BgiState
    |
    +-- keyQueue              std::queue<int>           Translated key codes (0-255 FIFO)
    +-- keyDown               std::array<uint8_t, 512>  Per-GLFW-key press state
    +-- mouseX                int                        Cursor X in window pixels
    +-- mouseY                int                        Cursor Y in window pixels
    +-- mouseMoved            bool                       True after cursor moves; cleared by API
    +-- selectedObjectIds     std::vector<string>        IDs of selected DDS objects
    +-- selectionPickRadiusPx int                        Pick hit radius in screen pixels (def 16)
    +-- selectionFlashScheme  int                        Flash colour: 0=orange, 1=purple
    +-- solidColorOverride    int                        >= 0 overrides face/edge for flash
    +-- userKeyHook           WxbgiKeyHook               User key hook pointer (nullptr = off)
    +-- userCharHook          WxbgiCharHook              User char hook pointer (nullptr = off)
    +-- userCursorPosHook     WxbgiCursorPosHook         User cursor hook pointer (nullptr = off)
    +-- userMouseButtonHook   WxbgiMouseButtonHook       User mouse button hook (nullptr = off)

src/bgi_state.h
    +-- gMutex            std::mutex                 Global non-recursive state lock
    +-- gState            BgiState                   Singleton state object
```

### Keyboard Layer

```
GLFW OS event
    |
    v
keyCallback()                      src/bgi_api.cpp
    |   Fires: key press, repeat, release
    |   [1] Writes: gState.keyDown[key] (all actions)
    |   [2] Pushes: gState.keyQueue     (press/repeat, special/control keys only)
    |   [3] Calls:  gState.userKeyHook  (all actions, if non-null)
    |
    +-- queueKeyCode(int)           pushes 1 int to keyQueue
    +-- queueExtendedKey(int)       pushes {0, scancode} to keyQueue
    |
    v
charCallback()                     src/bgi_api.cpp
    |   Fires: printable Unicode input (OS-composed, layout-aware)
    |   [1] Filters: drops codepoints >255 and 9/13/27
    |   [2] Pushes: gState.keyQueue (codepoint as-is)
    |   [3] Calls:  gState.userCharHook (if non-null, after queue push)
    |
    v
gState.keyQueue   std::queue<int>
gState.keyDown[]  std::array<uint8_t, 512>
    |
    v
Public API (src/bgi_modern_api.cpp, all acquire gMutex)
    |
    +-- wxbgi_key_pressed()                   check keyQueue.empty()
    +-- wxbgi_read_key()                       dequeue front; -1 if empty
    +-- wxbgi_is_key_down(key)                query keyDown[key], bypass queue
    +-- wxbgi_set_key_hook(cb)                store userKeyHook
    +-- wxbgi_set_char_hook(cb)               store userCharHook
    |
    +-- wxbgi_test_clear_key_queue()          (#ifdef WXBGI_ENABLE_TEST_SEAMS)
    +-- wxbgi_test_inject_key_code(int)       (#ifdef WXBGI_ENABLE_TEST_SEAMS)
    +-- wxbgi_test_inject_extended_scan(int)  (#ifdef WXBGI_ENABLE_TEST_SEAMS)
```

### Mouse Layer

```
GLFW OS event
    |
    v
cursorPosCallback()                src/bgi_api.cpp
    |   Fires: cursor position change (pixels, top-left origin)
    |   [1] Writes: gState.mouseX, gState.mouseY (truncated from double)
    |   [2] Writes: gState.mouseMoved = true
    |   [3] Calls:  gState.userCursorPosHook (if non-null)
    |
    v
mouseButtonCallback()              src/bgi_api.cpp
    |   Fires: any mouse button press or release
    |   [1] Left press only: overlayPerformPick(mouseX, mouseY, ctrl)
    |           -> gState.selectedObjectIds updated (add/toggle/deselect)
    |   [2] Calls: gState.userMouseButtonHook (if non-null, all buttons/actions)
    |
    v
overlayPerformPick()               src/bgi_overlay.cpp
    |   Iterates cameras with selCursorOverlay.enabled
    |   screenDistToObject() -- screen-space distance to each DDS object
    |   Sort by depth + distance; update gState.selectedObjectIds
    |
    v
Public API (src/bgi_modern_api.cpp, all acquire gMutex)
    |
    +-- wxbgi_get_mouse_pos(x, y)          read mouseX/mouseY
    +-- wxbgi_mouse_moved()                read + clear mouseMoved flag
    +-- wxbgi_set_cursor_pos_hook(cb)      store userCursorPosHook
    +-- wxbgi_set_mouse_button_hook(cb)    store userMouseButtonHook
```

### Event Pump Layer

```
Programmer main loop
    |
    v
wxbgi_poll_events()                src/bgi_modern_api.cpp
    |   [1] Acquires: gMutex
    |   [2] Calls:    glfwPollEvents()  <-- all callbacks + user hooks fire here
    |   [3] Returns:  0 on success, -1 if window not ready
    |
    v
wxbgi_begin_advanced_frame()       src/bgi_modern_api.cpp
    |   Also calls glfwPollEvents() (inside locked section)
    |   Used by: custom frame-rate-controlled render loops

Execution order within glfwPollEvents() for each event:
    [Library logic: keyDown/queue/mouseXY/pick] --> [User hook if non-null]
    All executing without mutex (already held by wxbgi_poll_events caller)
```

### File Dependency Diagram

```
wx_bgi_ext.h                   <-- public input API + hook typedefs + WXBGI_* constants
    |                              WxbgiKeyHook, WxbgiCharHook, WxbgiCursorPosHook,
    |                              WxbgiMouseButtonHook defined in bgi_types.h (via include)
    v
bgi_modern_api.cpp             <-- implements wxbgi_key_pressed, wxbgi_read_key,
    |                              wxbgi_is_key_down, wxbgi_get_mouse_pos, wxbgi_mouse_moved,
    |                              wxbgi_poll_events, wxbgi_set_*_hook(),
    |                              wxbgi_test_inject_* (WXBGI_ENABLE_TEST_SEAMS)
    +-- bgi_state.h            <-- gMutex, gState (BgiState singleton)
    +-- bgi_draw.h             <-- bgi::flushToScreen()
    +-- GLFW/glfw3.h           <-- glfwPollEvents()

bgi_api.cpp                    <-- GLFW callback implementations + initializeWindow()
    |                              keyCallback, charCallback, cursorPosCallback,
    |                              mouseButtonCallback (all call user hooks at end)
    +-- bgi_state.h            <-- gState, gMutex
    +-- bgi_overlay.h          <-- overlayPerformPick()
    +-- GLFW/glfw3.h           <-- glfwSet*Callback(), GLFW_KEY_*, GLFW_MOD_*

bgi_overlay.cpp                <-- overlayPerformPick(), screenDistToObject()
    +-- bgi_types.h            <-- BgiState (selectedObjectIds, mouseX/Y, etc.)
    +-- bgi_state.h            <-- gState, gMutex
    +-- bgi_camera.h           <-- camera view/proj matrix math (GLM)
    +-- bgi_dds.h              <-- DDS object iteration for pick candidates

bgi_types.h                    <-- BgiState struct + WxbgiKeyHook/CharHook/CursorPosHook/
    |                              MouseButtonHook typedefs (global scope, BGI_CALL)
    v
bgi_state.h / bgi_state.cpp    <-- gState singleton, gMutex definition,
                                   resetStateForWindow() (clears keyboard+mouse+hook state)
```

---

*See also:*
- **[VisualAids.md](./VisualAids.md)** — selection flash feedback, overlay pick system, full selection API reference
- **[Camera3D_Map.md](./Camera3D_Map.md)** — camera viewport math used by the click-to-pick pipeline
- **`src/wx_bgi_ext.h`** — complete public declarations with Doxygen doc-comments
- **`examples/cpp/wxbgi_keyboard_queue.cpp`** — live example of the keyboard queue API

---

## Test and Example Programs

### Automated Tests

#### `test_input_hooks` (C++) — `examples/cpp/test_input_hooks.cpp`

Fully automated test for all four user hook types. Registered as a CTest test.

**Phase 1** (always): verifies that `wxbgi_set_key_hook` / `wxbgi_set_char_hook` /
`wxbgi_set_cursor_pos_hook` / `wxbgi_set_mouse_button_hook` and their NULL-deregistration
counterparts complete without crashing.

**Phase 2** (requires `WXBGI_ENABLE_TEST_SEAMS`): uses the simulation seams to drive the
full callback → hook pipeline:
- Simulates key press, repeat, and release; verifies hook fires and `wxbgi_is_key_down` is updated.
- Simulates Up arrow; verifies the DOS extended `{0, 72}` sequence appears in `keyQueue`.
- Simulates char input (`'H'`); verifies hook fires and char is in `keyQueue`.
- Verifies ESC / out-of-range codepoints are silently dropped (hook NOT called, queue NOT updated).
- Simulates cursor moves; verifies hook fires and `wxbgi_get_mouse_pos` / `wxbgi_mouse_moved` reflect the new position.
- Simulates left, right, and release mouse button events with modifier keys.
- Verifies that setting a hook to `nullptr` stops it from firing.

**Phase 3** (always): draws summary primitives (circle at last simulated cursor position, colored
rectangles sized by fire counts) to confirm the BGI drawing pipeline is unaffected.

Build this test with `WXBGI_ENABLE_TEST_SEAMS=ON` for the full simulation phase:
```powershell
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

#### `test_input_hooks_pascal` (FreePascal) — `examples/demoFreePascal/test_input_hooks.pas`

Pascal mirror of the C++ test. Uses `{$IFDEF WXBGI_ENABLE_TEST_SEAMS}` to gate the simulation
phase. When CMake is configured with `-DWXBGI_ENABLE_TEST_SEAMS=ON`, the FPC compiler receives
`-dWXBGI_ENABLE_TEST_SEAMS` automatically. Two CTest tests are registered:
`test_input_hooks_pascal_build` (compiles the Pascal program) and
`test_input_hooks_pascal_run` (runs it).

Hook typedefs in Pascal (cdecl calling convention):
```pascal
type
  TWxbgiKeyHook         = procedure(key, scancode, action, mods: LongInt); cdecl;
  TWxbgiCharHook        = procedure(codepoint: LongWord); cdecl;
  TWxbgiCursorPosHook   = procedure(x, y: LongInt); cdecl;
  TWxbgiMouseButtonHook = procedure(button, action, mods: LongInt); cdecl;
```

### Interactive Demos

#### `wxbgi_input_hooks_cpp` (C++) — `examples/cpp/wxbgi_input_hooks.cpp`

Interactive demo showing all four hook types firing in real time. Not a CTest test — run it
manually from the build output directory.

| Input | Effect |
|---|---|
| Arrow keys | Move red crosshair marker |
| `C` key | Cycle stamp colour |
| Typed printable chars | Appear in text bar (Backspace removes last) |
| Mouse move | Yellow crosshair follows cursor |
| Left click | Stamp filled circle at cursor |
| Right click | Clear all stamps |
| Escape | Exit |

Four status panels across the top show each hook's last event and fire count in real time.

#### `demo_input_hooks_pascal` (FreePascal) — `examples/demoFreePascal/demo_input_hooks.pas`

Pascal interactive demo with identical behaviour to the C++ demo. Build with:
```cmake
cmake --build build --target demo_input_hooks_pascal_build
```
Then run `pascal_input_hooks/demo_input_hooks[.exe]` from the build directory.

### Test Seam Functions (simulation API)

The following functions are exported only when `WXBGI_ENABLE_TEST_SEAMS=1`. They faithfully
replicate the internal callback pipeline so automated tests can exercise hooks without real OS input:

| Function | Replicates | State updated before hook fires |
|---|---|---|
| `wxbgi_test_simulate_key(key, scan, action, mods)` | `keyCallback` | `keyDown[key]`; `keyQueue` (special keys) |
| `wxbgi_test_simulate_char(codepoint)` | `charCallback` | `keyQueue` (filtered chars only) |
| `wxbgi_test_simulate_cursor(x, y)` | `cursorPosCallback` | `mouseX`, `mouseY`, `mouseMoved` |
| `wxbgi_test_simulate_mouse_button(btn, action, mods)` | `mouseButtonCallback` (hook path only) | *(no state change; overlayPerformPick omitted)* |

Note: `wxbgi_test_simulate_char` applies the same filter as `charCallback` — codepoints ≤ 0,
> 255, or equal to 9 / 13 / 27 are dropped entirely (hook not called, queue not updated).
This matches the real callback behaviour exactly.
