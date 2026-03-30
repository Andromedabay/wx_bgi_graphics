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

/** @} */  // wxbgi_ext_api
