/**
 * @file bgi_gl.cpp
 * @brief OpenGL pass implementation: page-texture, Phong solid, depth-tested lines.
 *
 * All GL objects are lazily created on the first call to glPassInit() and
 * destroyed by glPassDestroy().  The module is completely self-contained;
 * nothing outside this file touches the GL handles it manages.
 *
 * Thread safety: all public functions must be called with the GL context current
 * and bgi::gMutex already held by the caller.
 */

// NOTE: glew.h must be included before any OpenGL header.
#include <GL/glew.h>

#include "bgi_gl.h"
#include "bgi_gl_shaders.h"
#include "bgi_state.h"
#include "bgi_draw.h"
#include "bgi_overlay.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace bgi
{

// =============================================================================
// Internal GL object handles (file-scope, one set per GL context)
// =============================================================================

namespace
{

bool  g_inited        = false;
int   g_texW          = 0;
int   g_texH          = 0;

// Page-texture pass
GLuint g_pageTex      = 0;
GLuint g_pageVAO      = 0;
GLuint g_pageVBO      = 0;
GLuint g_pageProgram  = 0;

// Solid pass (flat + smooth programs share the same VAO layout)
GLuint g_solidVAO     = 0;
GLuint g_solidVBO     = 0;
GLuint g_flatProgram  = 0;
GLuint g_smoothProgram= 0;

// Line pass
GLuint g_lineVAO      = 0;
GLuint g_lineVBO      = 0;
GLuint g_lineProgram  = 0;

// ---------------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------------

GLuint compileShader(GLenum type, const char *src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512] = {};
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        fprintf(stderr, "[bgi_gl] Shader compile error: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint linkProgram(const char *vertSrc, const char *fragSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512] = {};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "[bgi_gl] Program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// Page-texture helpers
// ---------------------------------------------------------------------------

void initPageObjects(int w, int h)
{
    // Create RGBA texture sized to the canvas.
    glGenTextures(1, &g_pageTex);
    glBindTexture(GL_TEXTURE_2D, g_pageTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_texW = w;
    g_texH = h;

    // Fullscreen quad: NDC position + UV.
    // UV y is flipped so (0,0) maps to top-left (BGI convention).
    static const float quadVerts[] = {
    //  pos X   pos Y    uv X   uv Y
        -1.f,  -1.f,    0.f,   1.f,
         1.f,  -1.f,    1.f,   1.f,
         1.f,   1.f,    1.f,   0.f,
        -1.f,  -1.f,    0.f,   1.f,
         1.f,   1.f,    1.f,   0.f,
        -1.f,   1.f,    0.f,   0.f,
    };
    glGenVertexArrays(1, &g_pageVAO);
    glGenBuffers(1, &g_pageVBO);
    glBindVertexArray(g_pageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_pageVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    // layout 0: vec2 pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // layout 1: vec2 uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    g_pageProgram = linkProgram(kPageTexVertSrc, kPageTexFragSrc);
}

// ---------------------------------------------------------------------------
// Solid VAO/VBO helpers
// ---------------------------------------------------------------------------

void initSolidObjects()
{
    glGenVertexArrays(1, &g_solidVAO);
    glGenBuffers(1, &g_solidVBO);
    glBindVertexArray(g_solidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_solidVBO);
    // layout 0: vec3 pos (offset 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlVertex),
                          (void*)offsetof(GlVertex, px));
    glEnableVertexAttribArray(0);
    // layout 1: vec3 normal (offset 12)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GlVertex),
                          (void*)offsetof(GlVertex, nx));
    glEnableVertexAttribArray(1);
    // layout 2: vec3 color (offset 24)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GlVertex),
                          (void*)offsetof(GlVertex, r));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    g_flatProgram   = linkProgram(kFlatVertSrc,   kFlatFragSrc);
    g_smoothProgram = linkProgram(kSmoothVertSrc, kSmoothFragSrc);
}

// ---------------------------------------------------------------------------
// Line VAO/VBO helpers
// ---------------------------------------------------------------------------

void initLineObjects()
{
    glGenVertexArrays(1, &g_lineVAO);
    glGenBuffers(1, &g_lineVBO);
    glBindVertexArray(g_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_lineVBO);
    // layout 0: vec3 pos (offset 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlLineVertex),
                          (void*)offsetof(GlLineVertex, px));
    glEnableVertexAttribArray(0);
    // layout 1: vec3 color (offset 12)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GlLineVertex),
                          (void*)offsetof(GlLineVertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    g_lineProgram = linkProgram(kLineVertSrc, kLineFragSrc);
}

// ---------------------------------------------------------------------------
// Safe delete helpers
// ---------------------------------------------------------------------------

void deleteGlObjects()
{
    if (g_pageTex)       { glDeleteTextures(1, &g_pageTex);       g_pageTex       = 0; }
    if (g_pageVAO)       { glDeleteVertexArrays(1, &g_pageVAO);   g_pageVAO       = 0; }
    if (g_pageVBO)       { glDeleteBuffers(1, &g_pageVBO);         g_pageVBO       = 0; }
    if (g_pageProgram)   { glDeleteProgram(g_pageProgram);         g_pageProgram   = 0; }
    if (g_solidVAO)      { glDeleteVertexArrays(1, &g_solidVAO);  g_solidVAO      = 0; }
    if (g_solidVBO)      { glDeleteBuffers(1, &g_solidVBO);        g_solidVBO      = 0; }
    if (g_flatProgram)   { glDeleteProgram(g_flatProgram);         g_flatProgram   = 0; }
    if (g_smoothProgram) { glDeleteProgram(g_smoothProgram);       g_smoothProgram = 0; }
    if (g_lineVAO)       { glDeleteVertexArrays(1, &g_lineVAO);   g_lineVAO       = 0; }
    if (g_lineVBO)       { glDeleteBuffers(1, &g_lineVBO);         g_lineVBO       = 0; }
    if (g_lineProgram)   { glDeleteProgram(g_lineProgram);         g_lineProgram   = 0; }
    g_inited = false;
}

} // anonymous namespace

// =============================================================================
// Public implementation
// =============================================================================

void glPassInit(int w, int h)
{
    // Guard: glGenVertexArrays requires GL 3.0+.  If GLEW didn't load it (old
    // context or no current context), bail out silently.  The caller should
    // have already fallen back to legacyGlRender via the GLEW_VERSION_3_3 check.
    if (!glGenVertexArrays)
        return;

    if (g_inited)
        deleteGlObjects();

    initPageObjects(w, h);
    initSolidObjects();
    initLineObjects();
    g_inited = true;
}

void glPassResize(int w, int h)
{
    if (w == g_texW && h == g_texH)
        return;
    if (g_pageTex)
    {
        glBindTexture(GL_TEXTURE_2D, g_pageTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    g_texW = w;
    g_texH = h;
}

void glPassDestroy()
{
    if (g_inited)
        deleteGlObjects();
}

// ---------------------------------------------------------------------------
// renderPageAsTexture
// ---------------------------------------------------------------------------

void renderPageAsTexture(int w, int h)
{
    if (!g_inited)
        glPassInit(w, h);

    if (!g_pageTex || !g_pageProgram)
    {
        renderPageLegacyPoints(w, h);
        return;
    }

    glPassResize(w, h);

    // Build RGBA pixel data from the 8-bit indexed visual page buffer.
    const auto &buf = visualPageBuffer();
    const int   npx = w * h;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(npx) * 4);
    const ColorRGB bg = colorToRGB(gState.bkColor);
    for (int i = 0; i < npx; ++i)
    {
        ColorRGB c = colorToRGB(buf[static_cast<std::size_t>(i)]);
        rgba[static_cast<std::size_t>(i) * 4 + 0] = c.r;
        rgba[static_cast<std::size_t>(i) * 4 + 1] = c.g;
        rgba[static_cast<std::size_t>(i) * 4 + 2] = c.b;
        rgba[static_cast<std::size_t>(i) * 4 + 3] = 255u;
    }

    glBindTexture(GL_TEXTURE_2D, g_pageTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                    GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Clear colour + depth.
    glViewport(0, 0, w, h);
    glClearColor(bg.r / 255.f, bg.g / 255.f, bg.b / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw fullscreen quad.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(g_pageProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_pageTex);
    glUniform1i(glGetUniformLocation(g_pageProgram, "uTex"), 0);
    glBindVertexArray(g_pageVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Draw selection cursors (uses legacy GL but is fine as an overlay).
    drawSelectionCursorsGL();
    glFlush();
}

// ---------------------------------------------------------------------------
// renderSolidsGLPass
// ---------------------------------------------------------------------------

static void setPhongUniforms(GLuint prog, const LightState &light,
                              const glm::mat4 &mvp, const glm::vec3 &camPos)
{
    glm::mat4 model     = glm::mat4(1.f); // vertices already in world space
    glm::mat3 normMat   = glm::mat3(glm::transpose(glm::inverse(model)));
    glm::vec3 lightDir  = glm::normalize(glm::vec3(light.dirX, light.dirY, light.dirZ));
    glm::vec3 fillDir   = glm::normalize(glm::vec3(light.fillX, light.fillY, light.fillZ));

    glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"),      1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"),    1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(prog, "uNormMat"),  1, GL_FALSE, glm::value_ptr(normMat));
    glUniform3fv(glGetUniformLocation(prog, "uLightDir"),       1, glm::value_ptr(lightDir));
    glUniform1f (glGetUniformLocation(prog, "uAmbient"),        light.ambient);
    glUniform1f (glGetUniformLocation(prog, "uDiffuse"),        light.diffuse);
    glUniform1f (glGetUniformLocation(prog, "uSpecular"),       light.specular);
    glUniform1f (glGetUniformLocation(prog, "uShininess"),      light.shininess);
    glUniform3fv(glGetUniformLocation(prog, "uCamPos"),         1, glm::value_ptr(camPos));
    glUniform3fv(glGetUniformLocation(prog, "uFillLightDir"),   1, glm::value_ptr(fillDir));
    glUniform1f (glGetUniformLocation(prog, "uFillStrength"),   light.fillStrength);
}

void renderSolidsGLPass(const PendingGlRender &pending, int w, int h,
                        const LightState &light, const glm::mat4 &vp,
                        const glm::vec3 &camPos)
{
    if (!g_inited || !pending.hasSolids())
        return;

    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // If the light is view-space, rotate it by the view rotation.
    LightState effectiveLight = light;
    if (!light.worldSpace)
    {
        glm::vec3 d = glm::normalize(glm::vec3(vp * glm::vec4(light.dirX, light.dirY, light.dirZ, 0.f)));
        effectiveLight.dirX = d.x;
        effectiveLight.dirY = d.y;
        effectiveLight.dirZ = d.z;
    }

    glBindVertexArray(g_solidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_solidVBO);

    // --- Flat pass ---
    if (!pending.solidVerts.empty() && g_flatProgram)
    {
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(pending.solidVerts.size() * sizeof(GlVertex)),
                     pending.solidVerts.data(), GL_STREAM_DRAW);
        glUseProgram(g_flatProgram);
        setPhongUniforms(g_flatProgram, effectiveLight, vp, camPos);
        glDrawArrays(GL_TRIANGLES, 0,
                     static_cast<GLsizei>(pending.solidVerts.size()));
    }

    // --- Smooth pass ---
    if (!pending.smoothVerts.empty() && g_smoothProgram)
    {
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(pending.smoothVerts.size() * sizeof(GlVertex)),
                     pending.smoothVerts.data(), GL_STREAM_DRAW);
        glUseProgram(g_smoothProgram);
        setPhongUniforms(g_smoothProgram, effectiveLight, vp, camPos);
        glDrawArrays(GL_TRIANGLES, 0,
                     static_cast<GLsizei>(pending.smoothVerts.size()));
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
// renderWorldLinesGLPass
// ---------------------------------------------------------------------------

void renderWorldLinesGLPass(const PendingGlRender &pending, int w, int h,
                             const glm::mat4 &vp)
{
    if (!g_inited || !pending.hasLines() || !g_lineProgram)
        return;

    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(g_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_lineVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pending.lineVerts.size() * sizeof(GlLineVertex)),
                 pending.lineVerts.data(), GL_STREAM_DRAW);

    glUseProgram(g_lineProgram);
    glUniformMatrix4fv(glGetUniformLocation(g_lineProgram, "uMVP"),
                       1, GL_FALSE, glm::value_ptr(vp));
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(pending.lineVerts.size()));

    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------------------
// renderPageLegacyPoints — the old per-pixel GL_POINTS path
// ---------------------------------------------------------------------------

void renderPageLegacyPoints(int w, int h)
{
    const ColorRGB background = colorToRGB(gState.bkColor);

    glViewport(0, 0, w, h);
    glClearColor(background.r / 255.f, background.g / 255.f, background.b / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(w), static_cast<double>(h), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glPointSize(1.f);

    const auto &buffer = visualPageBuffer();
    const int   bkIdx  = normalizeColor(gState.bkColor);
    glBegin(GL_POINTS);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int ci = buffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                  static_cast<std::size_t>(x)];
            if (ci == bkIdx) continue;
            const ColorRGB c = colorToRGB(ci);
            glColor3ub(c.r, c.g, c.b);
            glVertex2i(x, y);
        }
    }
    glEnd();

    drawSelectionCursorsGL();
    glFlush();
}

} // namespace bgi
