#pragma once

#include "bgi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

/**
 * @file bgi_camera.h
 * @brief Internal GLM-based camera math for the camera viewport system.
 *
 * All functions operate on @c Camera3D values stored in @c BgiState::cameras.
 * The coordinate convention throughout is **Z-up, right-handed** (AutoCAD /
 * engineering style): the XY plane is the ground plane and +Z points upward.
 *
 * OpenGL uses a Y-up convention internally; the conversion is handled
 * transparently inside the view-matrix construction for 2-D cameras and is the
 * caller's responsibility for 3-D cameras (set @c upZ = 1 for the default
 * Z-up orientation).
 *
 * None of these functions are part of the public C API.  Consumers use the
 * @c wxbgi_cam_* / @c wxbgi_cam2d_* functions declared in wx_bgi_3d.h.
 */

namespace bgi
{

    // -------------------------------------------------------------------------
    // Viewport helpers
    // -------------------------------------------------------------------------

    /**
     * Returns the effective screen-space viewport rectangle (pixels) for a camera.
     * When cam.vpW == 0 the entire window (@p winW × @p winH) is used.
     */
    void cameraEffectiveViewport(const Camera3D &cam, int winW, int winH,
                                 int &x, int &y, int &w, int &h);

    /** Returns the aspect ratio (width / height) of the camera's effective viewport. */
    float cameraAspectRatio(const Camera3D &cam, int winW, int winH);

    // -------------------------------------------------------------------------
    // Matrix construction
    // -------------------------------------------------------------------------

    /**
     * Builds the view matrix from eye / target / up, or from the 2-D pan /
     * zoom / rotation fields when @c cam.is2D is @c true.
     */
    glm::mat4 cameraViewMatrix(const Camera3D &cam);

    /**
     * Builds the projection matrix.
     *
     * For orthographic cameras the extents are taken from @c orthoLeft …
     * @c orthoTop.  When all four are zero the extents are auto-computed from
     * @c worldHeight2d (and @p aspectRatio for the width).
     * For 2-D cameras (@c is2D == true) the ortho extents are derived from
     * @c pan2dX/Y, @c zoom2d, and @c worldHeight2d so that the view is always
     * aspect-correct.
     */
    glm::mat4 cameraProjMatrix(const Camera3D &cam, float aspectRatio);

    /** Returns the combined view-projection matrix (proj × view). */
    glm::mat4 cameraVPMatrix(const Camera3D &cam, float aspectRatio);

    // -------------------------------------------------------------------------
    // Coordinate utilities
    // -------------------------------------------------------------------------

    /**
     * Projects a world-space point to a screen pixel.
     *
     * Screen coordinates are returned with (0,0) at the top-left corner and Y
     * increasing downward, matching classic BGI and most windowing conventions.
     *
     * @param cam      Camera to use for projection.
     * @param winW     Full GLFW window width in pixels.
     * @param winH     Full GLFW window height in pixels.
     * @param wx,wy,wz World-space input point.
     * @param screenX  Output screen X pixel coordinate.
     * @param screenY  Output screen Y pixel coordinate.
     * @return         @c true if the point is within the view frustum (visible),
     *                 @c false if it is behind the camera or outside clip depth.
     */
    bool cameraWorldToScreen(const Camera3D &cam, int winW, int winH,
                             float wx, float wy, float wz,
                             float &screenX, float &screenY);

    /**
     * Like @ref cameraWorldToScreen but skips the NDC frustum boundary test.
     *
     * The returned screen coordinates may be outside the viewport rectangle
     * (negative or beyond winW/winH).  The only hard rejection is a point that
     * is truly behind the camera (clip.w ≤ 0 for perspective cameras).  This
     * variant is used by polygon and line renderers that rely on per-pixel
     * viewport clipping inside @c drawLineInternal / @c fillPolygonInternal,
     * enabling correct partial clipping when primitives straddle viewport edges.
     */
    bool cameraWorldToScreenForced(const Camera3D &cam, int winW, int winH,
                                   float wx, float wy, float wz,
                                   float &screenX, float &screenY);

    /**
     * Transform a world-space point to 4-D clip space via the camera's VP matrix.
     * Use this to prepare vertices for Sutherland-Hodgman clipping.
     */
    glm::vec4 cameraWorldToClip(const Camera3D &cam, int winW, int winH,
                                float wx, float wy, float wz);

    /**
     * Sutherland-Hodgman clip step: clip a convex polygon (in 4-D clip space)
     * against one half-space.  "Inside" is dot(plane, P) > 0.
     *
     * @param poly  Polygon vertices (clip-space 4-D homogeneous), modified in place.
     * @param plane Clip-plane coefficients (A,B,C,D): A·X+B·Y+C·Z+D·W > 0.
     */
    void clipPolyByPlane(std::vector<glm::vec4> &poly, const glm::vec4 &plane);

    /**
     * Clip a convex polygon in clip space against the near plane (Z+W>0) and
     * far plane (-Z+W>0).  Together these ensure W>0 (safe for perspective divide)
     * and Z within the camera depth range.
     */
    void clipPolyZPlanes(std::vector<glm::vec4> &poly);

    /**
     * Clip a LINE SEGMENT in clip space against one half-space in place.
     * Returns false (and leaves A/B undefined) when both endpoints are outside.
     */
    bool clipLineByPlane(glm::vec4 &A, glm::vec4 &B, const glm::vec4 &plane);

    /**
     * Clip a line segment against near and far Z planes.
     * Returns false when the segment is fully clipped away.
     */
    bool clipLineZPlanes(glm::vec4 &A, glm::vec4 &B);

    /**
     * Project a 4-D clip-space point (after SH clipping) to a screen pixel.
     *
     * No NDC X/Y boundary test — the caller relies on per-pixel viewport
     * clipping inside drawLineInternal / fillPolygonInternal.
     * Returns false only when clip.w <= 0 (should not occur after near clipping).
     */
    bool cameraClipToScreen(const Camera3D &cam, int winW, int winH,
                            const glm::vec4 &clip,
                            float &screenX, float &screenY);

    /**
     * Unprojects a screen pixel to a world-space ray.
     *
     * The ray origin is on the near plane; the direction is normalised.
     * Screen coordinates follow the same top-left / Y-down convention as
     * @ref cameraWorldToScreen.
     *
     * @param cam                 Camera to use for unprojection.
     * @param winW,winH           Full GLFW window dimensions.
     * @param screenX,screenY     Input screen pixel.
     * @param rayOx,rayOy,rayOz   Output ray origin (world space).
     * @param rayDx,rayDy,rayDz   Output normalised ray direction (world space).
     */
    void cameraScreenToRay(const Camera3D &cam, int winW, int winH,
                           float screenX, float screenY,
                           float &rayOx, float &rayOy, float &rayOz,
                           float &rayDx, float &rayDy, float &rayDz);

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------

    /** Writes a GLM mat4 into a caller-supplied column-major float[16] array. */
    void matToFloatArray(const glm::mat4 &m, float *out16);

} // namespace bgi
