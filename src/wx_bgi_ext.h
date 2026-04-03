#pragma once

#include "bgi_types.h"

/**
 * @file wx_bgi_ext.h
 * @brief Advanced non-BGI extension API for modern OpenGL workflows.
 *
 * All extension functions are prefixed with `wxbgi_` to avoid collisions with
 * classic BGI-compatible symbols.
 */

/** @defgroup wxbgi_ext_api Advanced OpenGL Extension API
 *  @brief Optional runtime/window/context helpers that complement classic BGI calls.
 *  @{
 */

/**
 * @brief Checks whether advanced API operations can safely run.
 *
 * This returns 1 when a window/context exists and has not been closed, or 0 otherwise.
 */
BGI_API int BGI_CALL wxbgi_is_ready(void);
/**
 * @brief Pumps pending OS/window events.
 *
 * Call this in custom frame loops to keep input, resize, and close events responsive.
 */
BGI_API int BGI_CALL wxbgi_poll_events(void);
/**
 * @brief Reports whether a translated keyboard key event is pending.
 *
 * Returns 1 when @ref wxbgi_read_key can consume a queued key, otherwise 0.
 * Queue contents are generated from GLFW key and character callbacks attached
 * to the active graphics window.
 */
BGI_API int BGI_CALL wxbgi_key_pressed(void);
/**
 * @brief Reads the next translated keyboard event from the internal queue.
 *
 * Returns a non-negative key code when available, or -1 when no key is pending.
 * Printable keys arrive as character codes. Special keys such as Escape, Enter,
 * Tab, and Backspace are mapped to their classic control values. Extended keys
 * follow DOS-style semantics by emitting 0 followed by the translated scan code
 * on the next read.
 */
BGI_API int BGI_CALL wxbgi_read_key(void);
/**
 * @brief Queries whether a raw GLFW key code is currently held down.
 *
 * @param key GLFW key code such as `GLFW_KEY_ESCAPE` or `GLFW_KEY_LEFT`.
 * @return 1 if the key is currently down, 0 if up, or -1 on invalid input.
 *
 * This bypasses the translated queue and is useful for real-time key state checks
 * alongside queued compatibility reads.
 */
BGI_API int BGI_CALL wxbgi_is_key_down(int key);

/**
 * @brief Returns the current mouse cursor position in window pixels.
 *
 * Writes the current mouse X and Y coordinates (in window pixels, origin
 * top-left) into @p x and @p y.  Either pointer may be NULL.
 * Values are updated by GLFW's cursor-position callback; the function does
 * not call @c glfwPollEvents() itself.
 */
BGI_API void BGI_CALL wxbgi_get_mouse_pos(int *x, int *y);

/**
 * @brief Returns 1 if the mouse cursor moved since the last call to this function.
 *
 * The movement flag is set by the GLFW cursor-position callback and cleared
 * each time this function is called.  Use it to decide whether to redraw a
 * frame that contains a mouse-tracking visual aid (e.g., the selection cursor).
 *
 * @return 1 if the mouse moved since the previous call, 0 otherwise.
 */
BGI_API int BGI_CALL wxbgi_mouse_moved(void);

// ---------------------------------------------------------------------------
// User Input Hook Callbacks
// ---------------------------------------------------------------------------

/**
 * @defgroup wxbgi_input_hooks_api User Input Hook Registration
 * @brief Register optional user-supplied callbacks that fire after the
 *        library has completed its own internal event processing.
 *
 * The library retains ownership of all GLFW callbacks.  These functions let
 * you attach supplementary logic to keyboard and mouse events without
 * interfering with the key queue, keyDown state, mouse position tracking,
 * or the click-to-pick selection system.
 *
 * **Execution model:** Each hook fires synchronously from within the
 * corresponding GLFW callback, which executes while the library's internal
 * mutex is held (inside `glfwPollEvents()`).
 *
 * @warning Hooks **must not** call any `wxbgi_*` function.  Doing so will
 *          deadlock because the non-recursive internal mutex is already held.
 *          Safe operations from a hook: update your own variables, set flags,
 *          call non-wxbgi code.
 *
 * Pass `NULL` to any registration function to remove a previously installed hook.
 * @{
 */

/// @name Action and button constants (mirror GLFW values)
/// Use these in hook implementations instead of including GLFW headers.
///@{
#define WXBGI_KEY_PRESS    1  ///< Key or button pressed
#define WXBGI_KEY_RELEASE  0  ///< Key or button released
#define WXBGI_KEY_REPEAT   2  ///< Key held down (auto-repeat)
#define WXBGI_MOUSE_LEFT   0  ///< Left mouse button
#define WXBGI_MOUSE_RIGHT  1  ///< Right mouse button
#define WXBGI_MOUSE_MIDDLE 2  ///< Middle mouse button
#define WXBGI_MOD_SHIFT    0x0001  ///< Shift modifier held
#define WXBGI_MOD_CTRL     0x0002  ///< Control modifier held
#define WXBGI_MOD_ALT      0x0004  ///< Alt modifier held
///@}

/**
 * @brief Registers a user hook invoked after each key press, repeat, or release.
 *
 * The hook receives the same parameters as the GLFW key callback:
 * @p key is a GLFW key constant (e.g. `GLFW_KEY_ESCAPE` = 256; plain ASCII
 * letters use their uppercase ASCII code), @p scancode is the raw OS scancode,
 * @p action is one of `WXBGI_KEY_PRESS` / `WXBGI_KEY_RELEASE` /
 * `WXBGI_KEY_REPEAT`, and @p mods is a bitfield of `WXBGI_MOD_SHIFT`,
 * `WXBGI_MOD_CTRL`, `WXBGI_MOD_ALT`.
 *
 * @param cb Function pointer to install, or NULL to remove the current hook.
 */
BGI_API void BGI_CALL wxbgi_set_key_hook(WxbgiKeyHook cb);

/**
 * @brief Registers a user hook invoked after each printable character input event.
 *
 * Fires only for printable characters (Unicode codepoints 1–255 accepted by this
 * library).  Control characters (Tab=9, Enter=13, Escape=27) are handled by the
 * key hook instead.
 *
 * @param cb Function pointer to install, or NULL to remove the current hook.
 */
BGI_API void BGI_CALL wxbgi_set_char_hook(WxbgiCharHook cb);

/**
 * @brief Registers a user hook invoked after every cursor position update.
 *
 * @p x and @p y are the cursor's new position in window pixels, origin at the
 * top-left corner (same coordinate system as `wxbgi_get_mouse_pos()`).
 *
 * @param cb Function pointer to install, or NULL to remove the current hook.
 */
BGI_API void BGI_CALL wxbgi_set_cursor_pos_hook(WxbgiCursorPosHook cb);

/**
 * @brief Registers a user hook invoked after each mouse button press or release.
 *
 * @p button is `WXBGI_MOUSE_LEFT`, `WXBGI_MOUSE_RIGHT`, or `WXBGI_MOUSE_MIDDLE`.
 * @p action is `WXBGI_KEY_PRESS` or `WXBGI_KEY_RELEASE`.
 * @p mods is a bitfield of modifier keys.
 *
 * Note: the library's own pick/selection logic runs before this hook is called,
 * so `gState.selectedObjectIds` already reflects the result of a left-click pick.
 *
 * @param cb Function pointer to install, or NULL to remove the current hook.
 */
BGI_API void BGI_CALL wxbgi_set_mouse_button_hook(WxbgiMouseButtonHook cb);

/**
 * @brief Registers a user hook invoked after each mouse scroll (wheel) event.
 *
 * @p xoffset is the horizontal scroll delta; @p yoffset is the vertical
 * scroll delta (positive = scroll up/forward).
 *
 * @param cb Function pointer to install, or NULL to remove the current hook.
 */
BGI_API void BGI_CALL wxbgi_set_scroll_hook(WxbgiScrollHook cb);

/** @} */

/**
 * @defgroup wxbgi_scroll_api Mouse Scroll API
 * @brief Functions for reading accumulated scroll wheel input.
 * @{
 */

/**
 * @brief Reads and atomically clears the accumulated scroll deltas.
 *
 * Returns the total horizontal and vertical scroll since the last call.
 * Both deltas are reset to 0.0 after this call.  Either output pointer
 * may be NULL if that axis is not needed.
 *
 * @param dx Receives accumulated horizontal scroll (positive = right).
 * @param dy Receives accumulated vertical scroll (positive = up/forward).
 */
BGI_API void BGI_CALL wxbgi_get_scroll_delta(double *dx, double *dy);

/** @} */

/**
 * @defgroup wxbgi_input_defaults Input Default-Behavior Control
 * @brief API for bypassing or re-enabling the library's built-in input processing.
 *
 * By default, the library performs the following actions inside its GLFW callbacks:
 *
 * | Flag | Default action |
 * |---|---|
 * | `WXBGI_DEFAULT_KEY_QUEUE` | Translate GLFW keys to DOS codes; push to `keyQueue`. |
 * | `WXBGI_DEFAULT_CURSOR_TRACK` | Write cursor position to `mouseX`/`mouseY`; set `mouseMoved`. |
 * | `WXBGI_DEFAULT_MOUSE_PICK` | On left-click, call `overlayPerformPick()` to update `selectedObjectIds`. |
 * | `WXBGI_DEFAULT_SCROLL_ACCUM` | Accumulate scroll deltas in `scrollDeltaX`/`scrollDeltaY`. |
 *
 * Clearing a flag disables only that default.  User hooks always fire regardless
 * of flags.  Call `wxbgi_set_input_defaults(WXBGI_DEFAULT_ALL)` to restore all
 * defaults; `wxbgi_set_input_defaults(WXBGI_DEFAULT_NONE)` to disable all.
 * @{
 */

/**
 * @brief Sets the active input default-behavior flags.
 *
 * @param flags Bitwise OR of `WXBGI_DEFAULT_*` constants
 *              (e.g. `WXBGI_DEFAULT_ALL & ~WXBGI_DEFAULT_MOUSE_PICK`
 *              to keep all defaults except object picking).
 */
BGI_API void BGI_CALL wxbgi_set_input_defaults(int flags);

/**
 * @brief Returns the current input default-behavior bitmask.
 *
 * @return Bitwise OR of active `WXBGI_DEFAULT_*` flags.
 */
BGI_API int BGI_CALL wxbgi_get_input_defaults(void);

/** @} */

/**
 * @defgroup wxbgi_hook_context Hook-Context DDS Functions
 * @brief Functions safe to call from within user hook callbacks.
 *
 * All other `wxbgi_*` functions acquire `gMutex` and will deadlock if called
 * from a hook callback (which fires with the mutex already held).  The
 * `wxbgi_hk_*` functions intentionally skip mutex acquisition and are
 * designed specifically for use inside hook callbacks.
 *
 * @warning Call `wxbgi_hk_*` functions **only** from within a registered
 *          `WxbgiKeyHook`, `WxbgiCharHook`, `WxbgiCursorPosHook`,
 *          `WxbgiMouseButtonHook`, or `WxbgiScrollHook` callback.
 *          Calling them from outside a hook (where the mutex is NOT held)
 *          introduces a data race.
 * @{
 */

/** @brief Returns the current mouse X position (hook-context safe). */
BGI_API int BGI_CALL wxbgi_hk_get_mouse_x(void);
/** @brief Returns the current mouse Y position (hook-context safe). */
BGI_API int BGI_CALL wxbgi_hk_get_mouse_y(void);

/** @brief Returns the number of currently selected DDS objects (hook-context safe). */
BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_count(void);

/**
 * @brief Copies the ID of the nth selected object into @p outId (hook-context safe).
 *
 * @param index  Zero-based index into the selection list.
 * @param outId  Buffer to receive the null-terminated ID string; may be NULL.
 * @param maxLen Capacity of @p outId buffer including the null terminator.
 * @return Length of the full ID string, or -1 if @p index is out of range.
 */
BGI_API int BGI_CALL wxbgi_hk_dds_get_selected_id(int index, char *outId, int maxLen);

/**
 * @brief Returns 1 if the DDS object with the given ID is selected (hook-context safe).
 * @param id Object ID string; may be NULL (returns 0).
 */
BGI_API int BGI_CALL wxbgi_hk_dds_is_selected(const char *id);

/**
 * @brief Adds the DDS object with the given ID to the selection (hook-context safe).
 *
 * Has no effect if the object is already selected or @p id is NULL.
 */
BGI_API void BGI_CALL wxbgi_hk_dds_select(const char *id);

/**
 * @brief Removes the DDS object with the given ID from the selection (hook-context safe).
 *
 * Has no effect if the object is not selected or @p id is NULL.
 */
BGI_API void BGI_CALL wxbgi_hk_dds_deselect(const char *id);

/** @brief Clears the entire DDS selection (hook-context safe). */
BGI_API void BGI_CALL wxbgi_hk_dds_deselect_all(void);

/**
 * @brief Manually runs the DDS pick algorithm at screen position (@p x, @p y)
 *        and updates @p selectedObjectIds (hook-context safe).
 *
 * Equivalent to the left-click pick that `WXBGI_DEFAULT_MOUSE_PICK` normally
 * triggers.  Useful when that default is disabled and you want to implement
 * your own pick logic before calling this.
 *
 * @param x    Screen X in window pixels.
 * @param y    Screen Y in window pixels.
 * @param ctrl Non-zero to toggle (Ctrl+click behaviour); 0 for single select.
 * @return Number of selected objects after the pick.
 */
BGI_API int BGI_CALL wxbgi_hk_dds_pick_at(int x, int y, int ctrl);

/** @} */

/**
 * @brief Optional internal test seam APIs.
 *
 * These APIs are compiled only when `WXBGI_ENABLE_TEST_SEAMS` is defined at
 * build time. They are intended for deterministic CI/system testing and should
 * never be enabled in public production/release builds.
 */
#ifdef WXBGI_ENABLE_TEST_SEAMS
/**
 * @brief Internal test seam: clears pending translated keyboard queue entries.
 *
 * This is intended for automated tests only so CI can verify keyboard queue
 * behavior without relying on host OS input injection.
 */
BGI_API int BGI_CALL wxbgi_test_clear_key_queue(void);
/**
 * @brief Internal test seam: injects one translated key code into the queue.
 *
 * This bypasses GLFW callbacks and is intended for deterministic automated tests.
 */
BGI_API int BGI_CALL wxbgi_test_inject_key_code(int keyCode);
/**
 * @brief Internal test seam: injects an extended DOS-style key sequence.
 *
 * Enqueues `0` followed by @p scanCode to mirror translated extended key reads.
 */
BGI_API int BGI_CALL wxbgi_test_inject_extended_scan(int scanCode);

/**
 * @brief Test seam: simulate the full `keyCallback` pipeline.
 *
 * Updates `gState.keyDown[key]` (all actions), translates special/control
 * keys into queue entries (press/repeat only), then calls the registered
 * `userKeyHook` if set — faithfully replicating what `keyCallback` does
 * without requiring a real GLFW key event.
 *
 * @param key      GLFW key constant (e.g. `GLFW_KEY_A` = 65).
 * @param scancode OS scancode (passed to hook; not used for queue logic).
 * @param action   `WXBGI_KEY_PRESS`, `WXBGI_KEY_REPEAT`, or `WXBGI_KEY_RELEASE`.
 * @param mods     Modifier bitfield (e.g. `WXBGI_MOD_SHIFT`).
 * @return 0 on success, -1 if no window is open.
 */
BGI_API int BGI_CALL wxbgi_test_simulate_key(int key, int scancode, int action, int mods);

/**
 * @brief Test seam: simulate the full `charCallback` pipeline.
 *
 * Applies the same codepoint filter as `charCallback` (drops codepoints
 * outside 1–255 and drops Tab/Enter/Escape without calling the hook),
 * pushes accepted codepoints into `keyQueue`, then calls the registered
 * `userCharHook` if set.
 *
 * @param codepoint Unicode codepoint to simulate (1–255 accepted).
 * @return 0 on success, -1 if no window is open.
 */
BGI_API int BGI_CALL wxbgi_test_simulate_char(unsigned int codepoint);

/**
 * @brief Test seam: simulate the full `cursorPosCallback` pipeline.
 *
 * Writes @p x / @p y into `gState.mouseX` / `gState.mouseY`, sets
 * `gState.mouseMoved = true`, then calls the registered
 * `userCursorPosHook` if set.
 *
 * @param x New cursor X position in window pixels (top-left origin).
 * @param y New cursor Y position in window pixels.
 * @return 0 on success, -1 if no window is open.
 */
BGI_API int BGI_CALL wxbgi_test_simulate_cursor(int x, int y);

/**
 * @brief Test seam: simulate the full `mouseButtonCallback` pipeline.
 *
 * Calls the registered `userMouseButtonHook` if set.  The DDS pick logic
 * (`overlayPerformPick`) is intentionally omitted — it requires a valid
 * rendered framebuffer that is not available in headless test contexts.
 *
 * @param button Mouse button index (`WXBGI_MOUSE_LEFT`, etc.).
 * @param action `WXBGI_KEY_PRESS` or `WXBGI_KEY_RELEASE`.
 * @param mods   Modifier bitfield.
 * @return 0 on success, -1 if no window is open.
 */
BGI_API int BGI_CALL wxbgi_test_simulate_mouse_button(int button, int action, int mods);

/**
 * @brief Test seam: simulate the full `scrollCallback` pipeline.
 *
 * Accumulates @p xoffset / @p yoffset into `gState.scrollDeltaX/Y` (if
 * `WXBGI_DEFAULT_SCROLL_ACCUM` is active), then calls the registered
 * `userScrollHook` if set — faithfully replicating what `scrollCallback` does.
 *
 * @param xoffset Horizontal scroll delta.
 * @param yoffset Vertical scroll delta (positive = up/forward).
 * @return 0 on success, -1 if no window is open.
 */
BGI_API int BGI_CALL wxbgi_test_simulate_scroll(double xoffset, double yoffset);
#endif
/**
 * @brief Reports whether the window received a close request.
 *
 * Returns 1 when closing is pending, otherwise 0.
 */
BGI_API int BGI_CALL wxbgi_should_close(void);
/**
 * @brief Requests closing the active graphics window.
 *
 * This marks the window for shutdown so your loop can exit cleanly.
 */
BGI_API void BGI_CALL wxbgi_request_close(void);

/**
 * @brief Makes the library OpenGL context current on this thread.
 *
 * Use this before raw OpenGL calls when your code can run across multiple threads.
 */
BGI_API int BGI_CALL wxbgi_make_context_current(void);
/**
 * @brief Swaps front/back window buffers immediately.
 *
 * This presents the back buffer to screen for custom rendering loops.
 */
BGI_API int BGI_CALL wxbgi_swap_window_buffers(void);
/**
 * @brief Enables or disables vertical synchronization.
 *
 * @param enabled Non-zero enables vsync, zero disables it.
 */
BGI_API int BGI_CALL wxbgi_set_vsync(int enabled);

/**
 * @brief Retrieves window size in screen coordinates.
 *
 * @param width Receives current width.
 * @param height Receives current height.
 */
BGI_API int BGI_CALL wxbgi_get_window_size(int *width, int *height);
/**
 * @brief Retrieves framebuffer size in physical pixels.
 *
 * This can differ from window size on HiDPI displays.
 */
BGI_API int BGI_CALL wxbgi_get_framebuffer_size(int *width, int *height);
/**
 * @brief Updates the native window title text.
 *
 * @param title UTF-8 title string.
 */
BGI_API int BGI_CALL wxbgi_set_window_title(const char *title);

/**
 * @brief Returns the GLFW monotonic timer in seconds.
 *
 * Useful for animation deltas, profiling, and frame timing.
 */
BGI_API double BGI_CALL wxbgi_get_time_seconds(void);
/**
 * @brief Resolves an OpenGL function pointer by symbol name.
 *
 * Returns null when the symbol is unavailable in the active context.
 */
BGI_API void *BGI_CALL wxbgi_get_proc_address(const char *procName);

/**
 * @brief Returns OpenGL implementation strings.
 * @param which 0=vendor, 1=renderer, 2=version, 3=shading language version.
 * @return C string for requested token, or empty string on error.
 */
BGI_API const char *BGI_CALL wxbgi_get_gl_string(int which);

/**
 * @brief Begins a manual OpenGL frame.
 *
 * Optionally clears color and/or depth buffers, updates viewport to framebuffer
 * dimensions, and polls events. This is intended for advanced rendering paths
 * that mix custom GL with classic BGI drawing.
 */
BGI_API int BGI_CALL wxbgi_begin_advanced_frame(float r, float g, float b, float a, int clearColor, int clearDepth);
/**
 * @brief Ends a manual frame.
 *
 * Flushes GL commands and optionally swaps buffers for presentation.
 */
BGI_API int BGI_CALL wxbgi_end_advanced_frame(int swapBuffers);

/**
 * @brief Reads RGBA8 pixels from the current framebuffer.
 *
 * This is useful for screenshots, pixel picking, and post-processing capture.
 * @return Number of bytes written, or a negative value on error.
 */
BGI_API int BGI_CALL wxbgi_read_pixels_rgba8(int x, int y, int width, int height, unsigned char *outBuffer, int outBufferSize);

/**
 * @brief Writes RGBA8 pixels into the current framebuffer.
 *
 * This replaces a rectangular region with caller-provided pixel data and is
 * useful for texture-less blits, overlays, and framebuffer restore operations.
 * @return Number of bytes consumed, or a negative value on error.
 */
BGI_API int BGI_CALL wxbgi_write_pixels_rgba8(int x, int y, int width, int height, const unsigned char *inBuffer, int inBufferSize);

/**
 * @brief Assigns an RGB colour to a specific extended palette slot (index 16-255).
 *
 * The classic 16-colour BGI palette (indices 0-15) is unaffected; use the
 * standard @ref setrgbpalette for those entries.
 *
 * @param idx   Extended palette slot to define, must be in the range [16, 255].
 * @param r,g,b Colour components (0-255 each).
 * @return @p idx on success, or -1 if @p idx is outside [16, 255].
 */
BGI_API int  BGI_CALL wxbgi_define_color(int idx, int r, int g, int b);

/**
 * @brief Allocates the next free extended palette slot and assigns it an RGB colour.
 *
 * Slots are assigned sequentially starting at index 16.  Use @ref wxbgi_free_color
 * to release slots for reuse.
 *
 * @param r,g,b Colour components (0-255 each).
 * @return Allocated palette index (16-255), or -1 if all 240 extended slots are exhausted.
 */
BGI_API int  BGI_CALL wxbgi_alloc_color(int r, int g, int b);

/**
 * @brief Releases an extended palette slot so it may be reused by wxbgi_alloc_color.
 *
 * The slot is reset to black (0,0,0).  Has no effect on classic palette indices 0-15.
 * @param idx Extended palette slot to release (16-255).
 */
BGI_API void BGI_CALL wxbgi_free_color(int idx);

/**
 * @brief Retrieves the RGB components of any colour index (0-255).
 *
 * Works for both classic BGI palette entries (0-15, including any setrgbpalette
 * overrides) and user-defined extended entries (16-255).
 *
 * @param color Palette index 0-255.
 * @param r     Receives the red component (0-255).
 * @param g     Receives the green component (0-255).
 * @param b     Receives the blue component (0-255).
 */
BGI_API void BGI_CALL wxbgi_getrgb(int color, int *r, int *g, int *b);



// ---------------------------------------------------------------------------

// PNG framebuffer export

// ---------------------------------------------------------------------------



/**

 * @defgroup wxbgi_export_api PNG Export API

 * @brief Save the BGI framebuffer or a DDS camera view directly to a PNG file.

 *

 * All three functions use a self-contained PNG encoder (CRC-32, Adler-32,

 * deflate stored blocks) - no external image library is required.

 * @{

 */



/**

 * @brief Export the current visual-page pixel buffer to a PNG file.

 *

 * Captures exactly what is currently displayed (the visual page).

 * Output dimensions match the window size passed to @c initwindow().

 *

 * @param filePath Destination file path (e.g. @c "screenshot.png").

 * @return 0 on success, -1 if no window is open or the file cannot be written.

 */

BGI_API int BGI_CALL wxbgi_export_png(const char *filePath);



/**

 * @brief Export the full OpenGL window content to a PNG file.

 *

 * For pure BGI contexts equivalent to @ref wxbgi_export_png.

 * Provided as a distinct entry point for callers that conceptually want

 * "everything displayed in the window" regardless of page selection.

 *

 * @param filePath Destination file path.

 * @return 0 on success, -1 on error.

 */

BGI_API int BGI_CALL wxbgi_export_png_window(const char *filePath);



/**

 * @brief Render the DDS scene through a camera and export its viewport to PNG.

 *

 * Calls @ref wxbgi_render_dds internally, then crops the active-page buffer to

 * the named camera's viewport rectangle and writes the result as a PNG.

 * Pass @c NULL as @p camName to use the active camera.

 * If the camera has no explicit viewport the full window is exported.

 *

 * @param camName  Name of the camera to render through (or @c NULL).

 * @param filePath Destination file path.

 * @return 0 on success, -1 on error.

 */

BGI_API int BGI_CALL wxbgi_export_png_camera_view(const char *camName,

                                                   const char *filePath);



/** @} */  // wxbgi_export_api

/** @defgroup wxbgi_wx_api wxWidgets Embedding API
 *  @brief C API functions for hosting the BGI surface inside a wxGLCanvas.
 *
 *  These functions are used by @c WxBgiCanvas (and any custom GL canvas)
 *  to bridge the wx-side event handlers and the BGI state managed inside
 *  @c wx_bgi_opengl.dll.  All locking is handled internally.
 *  @{
 */

/**
 * @brief Initialise BGI state for wxWidgets-embedded mode (no GLFW).
 *
 * Call once from your @c WxBgiCanvas constructor or for headless testing.
 * Allocates CPU page-buffers and registers the default camera/UCS.
 * The actual GL context must already be current before calling
 * @c wxbgi_wx_render_page_gl().
 */
BGI_API void BGI_CALL wxbgi_wx_init_for_canvas(int width, int height);

/**
 * @brief Render the BGI pixel buffer using the currently active OpenGL context.
 *
 * Call from inside your @c OnPaint / @c EVT_PAINT handler, after
 * @c SetCurrent(*glContext).  Locks the BGI mutex internally.
 */
BGI_API void BGI_CALL wxbgi_wx_render_page_gl(int width, int height);

/**
 * @brief Notify BGI that the canvas has been resized.
 *
 * Call from @c EVT_SIZE.  Reallocates page buffers for the new dimensions.
 */
BGI_API void BGI_CALL wxbgi_wx_resize(int width, int height);

/**
 * @brief Get the current BGI canvas dimensions.
 */
BGI_API void BGI_CALL wxbgi_wx_get_size(int* width, int* height);

/**
 * @brief Inject a keyboard key press or release event into BGI.
 *
 * @param glfwKey  GLFW key code (or equivalent mapping).
 * @param action   1 = press (WXBGI_KEY_PRESS), 0 = release (WXBGI_KEY_RELEASE).
 */
BGI_API void BGI_CALL wxbgi_wx_key_event(int glfwKey, int action);

/**
 * @brief Inject a Unicode character event into the BGI key queue.
 *
 * @param codepoint  Unicode code point (1-255).
 */
BGI_API void BGI_CALL wxbgi_wx_char_event(int codepoint);

/**
 * @brief Inject a mouse-move event into BGI.
 */
BGI_API void BGI_CALL wxbgi_wx_mouse_move(int x, int y);

/**
 * @brief Inject a mouse-button press or release into BGI.
 *
 * @param btn     @c WXBGI_MOUSE_LEFT / @c WXBGI_MOUSE_RIGHT / @c WXBGI_MOUSE_MIDDLE.
 * @param action  @c WXBGI_KEY_PRESS or @c WXBGI_KEY_RELEASE.
 */
BGI_API void BGI_CALL wxbgi_wx_mouse_button(int btn, int action);

/**
 * @brief Inject a scroll event into BGI.
 *
 * @param xDelta  Horizontal scroll delta.
 * @param yDelta  Vertical scroll delta (positive = scroll up).
 */
BGI_API void BGI_CALL wxbgi_wx_scroll(double xDelta, double yDelta);

/** @} */  // wxbgi_wx_api

/**
 * @defgroup wxbgi_standalone_api Standalone wx Window API
 * @ingroup wxbgi_ext_api
 * @brief wxPython-style App/Frame/MainLoop API for Python, Pascal, and simple C programs.
 *
 * These functions create and run a wxWidgets window with an embedded
 * WxBgiCanvas without requiring the caller to write any C++ wxFrame subclass.
 *
 * Typical usage (analogous to wxPython):
 * @code
 * // C / Python ctypes / Pascal cdecl:
 * wxbgi_wx_app_create();
 * wxbgi_wx_frame_create(640, 480, "My BGI App");
 * // ... BGI drawing calls here (write to CPU offscreen buffer) ...
 * wxbgi_wx_close_after_ms(5000);   // for automated tests
 * wxbgi_wx_app_main_loop();        // blocking -- returns when window closes
 * @endcode
 *
 * @warning Do NOT call wxbgi_wx_app_create() from a C++ application that
 * already owns a wxApp via wxIMPLEMENT_APP.  In that case use the
 * wx_bgi_wx static library and WxBgiCanvas directly.
 * @{
 */

/** @brief Create the wx application instance. Analogous to @c wx.App() in wxPython.
 *  Must be called once before wxbgi_wx_frame_create(). Safe to call multiple times
 *  (idempotent if a wxApp already exists). */
BGI_API void BGI_CALL wxbgi_wx_app_create(void);

/** @brief Create a top-level wxFrame with an embedded WxBgiCanvas.
 *  Analogous to @c wx.Frame() in wxPython. Also initialises the BGI CPU page
 *  buffers (calls wxbgi_wx_init_for_canvas internally) so that BGI drawing
 *  functions can be called before wxbgi_wx_app_main_loop(). */
BGI_API void BGI_CALL wxbgi_wx_frame_create(int width, int height, const char* title);

/** @brief Run the wx event loop. Analogous to @c app.MainLoop() in wxPython.
 *  Blocks until all top-level windows are closed. Returns only after
 *  cleanup. */
BGI_API void BGI_CALL wxbgi_wx_app_main_loop(void);

/** @brief Close the standalone frame immediately. */
BGI_API void BGI_CALL wxbgi_wx_close_frame(void);

/** @brief Schedule the frame to close after @p ms milliseconds.
 *  Useful for automated tests that need a short visible window. */
BGI_API void BGI_CALL wxbgi_wx_close_after_ms(int ms);

/** @brief Callback type for animation / per-frame update. */
typedef void (BGI_CALL *WxbgiFrameCallback)(void);

/** @brief Register a per-frame callback. Called on each timer tick set by
 *  wxbgi_wx_set_frame_rate(). Draw your scene here; the canvas is
 *  automatically refreshed after the callback returns. Pass NULL to
 *  deregister. */
BGI_API void BGI_CALL wxbgi_wx_set_idle_callback(WxbgiFrameCallback fn);

/** @brief Set the auto-refresh rate in frames per second (default 0 = no
 *  auto-refresh). Must be called after wxbgi_wx_frame_create(). */
BGI_API void BGI_CALL wxbgi_wx_set_frame_rate(int fps);

/** @brief Request an immediate canvas repaint. */
BGI_API void BGI_CALL wxbgi_wx_refresh(void);

/** @} */ // wxbgi_standalone_api

/** @} */  // wxbgi_ext_api
