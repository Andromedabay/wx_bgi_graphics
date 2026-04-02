#pragma once
/**
 * @file bgi_gl.h
 * @brief Internal OpenGL state management and rendering passes for the BGI library.
 *
 * This module owns all GL object lifetimes (shaders, VAOs, VBOs, textures) and
 * provides three compositing passes called in order by both the GLFW standalone
 * path (bgi_draw.cpp::flushToScreen) and the wx-embedded path
 * (WxBgiCanvas::Render):
 *
 *  1. renderPageAsTexture()    — upload the 8-bit page buffer as RGBA and draw
 *                                a fullscreen textured quad.  Clears depth buffer.
 *  2. renderSolidsGLPass()     — draw collected DDS solid triangles with GLSL Phong
 *                                shading and GL_DEPTH_TEST enabled.
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

#include "bgi_types.h"   // bgi::LightState

#include <vector>

namespace bgi
{

// ---------------------------------------------------------------------------
// Per-vertex data collected during DDS traversal for the GL render pass
// ---------------------------------------------------------------------------

struct GlVertex
{
    float px, py, pz;        ///< world-space position
    float nx, ny, nz;        ///< world-space normal (face or smooth)
    float r,  g,  b;         ///< linear RGB colour (0–1)
};

struct GlLineVertex
{
    float px, py, pz;        ///< world-space position
    float r,  g,  b;         ///< linear RGB colour (0–1)
};

/** Accumulated geometry for a single wxbgi_render_dds() call. */
struct PendingGlRender
{
    std::vector<GlVertex>     solidVerts;   ///< flat-mode vertices (face normals)
    std::vector<GlVertex>     smoothVerts;  ///< smooth-mode vertices (vertex normals)
    std::vector<GlLineVertex> lineVerts;    ///< depth-tested world lines

    bool hasSolids() const { return !solidVerts.empty() || !smoothVerts.empty(); }
    bool hasLines()  const { return !lineVerts.empty(); }
    void clear()           { solidVerts.clear(); smoothVerts.clear(); lineVerts.clear(); }
};

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

// ---------------------------------------------------------------------------
// Rendering passes
// ---------------------------------------------------------------------------

/** Upload the visual page buffer as an RGBA texture and draw a fullscreen quad.
 *  Clears GL_DEPTH_BUFFER_BIT so subsequent solid/line passes have a fresh depth
 *  buffer.  Does NOT swap buffers.  Auto-initialises on first call. */
void renderPageAsTexture(int w, int h);

/** Draw all solid triangles in @p pending with Phong lighting and depth test.
 *  Uses the flat program for solidVerts, smooth program for smoothVerts.
 *  Does NOT swap buffers. */
void renderSolidsGLPass(const PendingGlRender &pending, int w, int h,
                        const LightState &light, const glm::mat4 &vp,
                        const glm::vec3 &camPos);

/** Draw all world-space line segments in @p pending with depth test.
 *  Does NOT swap buffers. */
void renderWorldLinesGLPass(const PendingGlRender &pending, int w, int h,
                             const glm::mat4 &vp);

/** Legacy per-pixel GL_POINTS path (kept for backward compat / diagnostics). */
void renderPageLegacyPoints(int w, int h);

} // namespace bgi
