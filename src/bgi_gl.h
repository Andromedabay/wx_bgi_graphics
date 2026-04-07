#pragma once
/**
 * @file bgi_gl.h
 * @brief Internal OpenGL state management and rendering passes for the BGI library.
 *
 * This module owns all GL object lifetimes (shaders, VAOs, VBOs, textures) and
 * provides compositing passes called in order by both the GLFW standalone path
 * (bgi_draw.cpp::flushToScreen) and the wx-embedded path (WxBgiCanvas::Render):
 *
 *  1. renderPageAsTexture()    — upload the 8-bit page buffer as RGBA and draw
 *                                a fullscreen textured quad.  Clears depth buffer.
 *  2a. renderSolidsGLPass()    — draw collected DDS solid triangles with GLSL Phong
 *                                shading and GL_DEPTH_TEST enabled.
 *  2b. renderWireframeGLPass() — two-pass hidden-line wireframe: depth-only solid
 *                                fill (Pass 1) then depth-tested edge lines (Pass 2).
 *  3. renderWorldLinesGLPass() — draw collected world-space line segments with
 *                                GL_DEPTH_TEST enabled (hidden behind solids).
 *
 * Thread safety: all functions must be called with the GL context current and
 * bgi::gMutex already held by the caller.
 *
 * Legacy path: call renderPageLegacyPoints() instead of renderPageAsTexture()
 * to reproduce the old per-pixel GL_POINTS behaviour (for diagnostic use).
 */

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "bgi_types.h"   // bgi::LightState, bgi::PendingGlRender, bgi::GlVertex, bgi::GlLineVertex

namespace bgi
{

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/** Initialise GL objects (shaders, VAO/VBO, page texture).
 *  Must be called once after a valid GL context is current and GLEW is inited.
 *  Safe to call multiple times — reinitialises if already initialised. */
void glPassInit(int w, int h);

/** Resize the page texture to match a new canvas size. */
void glPassResize(int w, int h);

/** Release all GL objects (call before context is destroyed). */
void glPassDestroy();

/** Reset GL object handles to zero WITHOUT calling GL delete functions.
 *  Call this after the GL context has already been destroyed (e.g., after
 *  closegraph() tears down the GLFW window) so that the next context can
 *  start fresh.  It is NOT safe to call GL functions after context destruction. */
void glPassResetState();

// ---------------------------------------------------------------------------
// Rendering passes (call in order for correct compositing)
// ---------------------------------------------------------------------------

/** Pass 1 — Upload the visual page buffer as an RGBA texture and draw a
 *  fullscreen quad.  Clears GL_DEPTH_BUFFER_BIT so subsequent 3-D passes
 *  have a fresh depth buffer.  Does NOT swap buffers.  Auto-initialises.
 *
 *  @param w,h     Page-buffer (logical) dimensions — used for the texture.
 *  @param vpW,vpH Physical viewport dimensions for glViewport.  Pass -1 to
 *                 use the page dimensions (default for non-DPI-scaled paths). */
void renderPageAsTexture(int w, int h, int vpW = -1, int vpH = -1);

/** Overlay pass — Upload the visual page buffer as an RGBA texture and
 *  composite it ON TOP of the current framebuffer contents using alpha
 *  blending.  Background-coloured pixels are rendered as fully transparent
 *  (alpha = 0) so only non-background overlay pixels appear.
 *
 *  Call AFTER all 3-D solid/line passes so overlays always appear in front.
 *  Does NOT clear the colour or depth buffer.
 *
 *  @param w,h     Page-buffer (logical) dimensions — used for the texture.
 *  @param vpW,vpH Physical viewport dimensions for glViewport.  Pass -1 to
 *                 use the page dimensions. */
void renderPageAsTextureAlpha(int w, int h, int vpW = -1, int vpH = -1);

/** Pass 2a — Draw Phong-lit solid triangles (flat and/or smooth shading) with
 *  depth test.  Uses flat program for solidVerts, smooth program for smoothVerts.
 *  vpX/vpY: GL-coord bottom-left of the target sub-panel (0,0 = full window). */
void renderSolidsGLPass(const PendingGlRender &pending, int w, int h,
                        const LightState &light, const glm::mat4 &vp,
                        const glm::vec3 &camPos, int vpX = 0, int vpY = 0);

/** Pass 2b — Two-pass hidden-line wireframe with proper depth-buffer HSR.
 *  Pass 1 fills the depth buffer with solid triangles (no colour output, back-face
 *  culled).  Pass 2 draws only the visible edges (depth write off, GL_LEQUAL).
 *  vpX/vpY: GL-coord bottom-left of the target sub-panel (0,0 = full window). */
void renderWireframeGLPass(const PendingGlRender &pending, int w, int h,
                            const glm::mat4 &vp, const glm::vec3 &camPos,
                            int vpX = 0, int vpY = 0);

/** Pass 3 — Draw world-space line segments with depth test (hidden behind solids).
 *  vpX/vpY: GL-coord bottom-left of the target sub-panel (0,0 = full window). */
void renderWorldLinesGLPass(const PendingGlRender &pending, int w, int h,
                             const glm::mat4 &vp, int vpX = 0, int vpY = 0);

/** Legacy per-pixel GL_POINTS path (kept for backward compat / diagnostics). */
void renderPageLegacyPoints(int w, int h, int vpW = -1, int vpH = -1);

} // namespace bgi

