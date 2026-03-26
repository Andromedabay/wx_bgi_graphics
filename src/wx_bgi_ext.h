#pragma once

#include "bgi_types.h"

// Advanced non-BGI API for modern OpenGL/game/scientific workflows.
// These functions are intentionally prefixed to avoid clashing with classic BGI names.

BGI_API int BGI_CALL wxbgi_is_ready(void);
BGI_API int BGI_CALL wxbgi_poll_events(void);
BGI_API int BGI_CALL wxbgi_should_close(void);
BGI_API void BGI_CALL wxbgi_request_close(void);

BGI_API int BGI_CALL wxbgi_make_context_current(void);
BGI_API int BGI_CALL wxbgi_swap_window_buffers(void);
BGI_API int BGI_CALL wxbgi_set_vsync(int enabled);

BGI_API int BGI_CALL wxbgi_get_window_size(int *width, int *height);
BGI_API int BGI_CALL wxbgi_get_framebuffer_size(int *width, int *height);
BGI_API int BGI_CALL wxbgi_set_window_title(const char *title);

BGI_API double BGI_CALL wxbgi_get_time_seconds(void);
BGI_API void *BGI_CALL wxbgi_get_proc_address(const char *procName);

// which: 0=vendor, 1=renderer, 2=version, 3=shading language version.
BGI_API const char *BGI_CALL wxbgi_get_gl_string(int which);

// Begin/end an advanced OpenGL frame.
BGI_API int BGI_CALL wxbgi_begin_advanced_frame(float r, float g, float b, float a, int clearColor, int clearDepth);
BGI_API int BGI_CALL wxbgi_end_advanced_frame(int swapBuffers);

// Read back RGBA8 pixels from the currently bound framebuffer.
// Returns number of bytes written, or negative on error.
BGI_API int BGI_CALL wxbgi_read_pixels_rgba8(int x, int y, int width, int height, unsigned char *outBuffer, int outBufferSize);
