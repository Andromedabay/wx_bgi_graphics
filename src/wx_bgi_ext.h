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

/** @} */
