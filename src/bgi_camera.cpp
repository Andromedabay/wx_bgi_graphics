#include "bgi_camera.h"

#include <algorithm>
#include <cmath>

namespace bgi
{

    // -------------------------------------------------------------------------
    // Viewport helpers
    // -------------------------------------------------------------------------

    void cameraEffectiveViewport(const Camera3D &cam, int winW, int winH,
                                 int &x, int &y, int &w, int &h)
    {
        if (cam.vpW > 0 && cam.vpH > 0)
        {
            // Clamp to the current window so camera viewports are always
            // well-defined and cannot spill outside device bounds.
            x = std::clamp(cam.vpX, 0, std::max(0, winW - 1));
            y = std::clamp(cam.vpY, 0, std::max(0, winH - 1));

            const int right  = std::clamp(cam.vpX + cam.vpW, 0, winW);
            const int bottom = std::clamp(cam.vpY + cam.vpH, 0, winH);

            w = std::max(0, right - x);
            h = std::max(0, bottom - y);
        }
        else
        {
            x = 0;
            y = 0;
            w = winW;
            h = winH;
        }
    }

    float cameraAspectRatio(const Camera3D &cam, int winW, int winH)
    {
        int x, y, w, h;
        cameraEffectiveViewport(cam, winW, winH, x, y, w, h);
        return (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.f;
    }

    // -------------------------------------------------------------------------
    // Matrix construction
    // -------------------------------------------------------------------------

    glm::mat4 cameraViewMatrix(const Camera3D &cam)
    {
        if (cam.is2D)
        {
            // 2-D camera: eye directly above the pan target looking straight
            // down along -Z (in Z-up world).  The up vector is rotated by
            // rot2dDeg so the view can be spun around the vertical axis.
            const float cosR = std::cos(glm::radians(cam.rot2dDeg));
            const float sinR = std::sin(glm::radians(cam.rot2dDeg));

            return glm::lookAt(
                glm::vec3(cam.pan2dX, cam.pan2dY, 1.f),   // eye (above XY plane)
                glm::vec3(cam.pan2dX, cam.pan2dY, 0.f),   // target (on XY plane)
                glm::vec3(-sinR, cosR, 0.f));              // up rotated in XY
        }

        return glm::lookAt(
            glm::vec3(cam.eyeX,    cam.eyeY,    cam.eyeZ),
            glm::vec3(cam.targetX, cam.targetY, cam.targetZ),
            glm::vec3(cam.upX,     cam.upY,     cam.upZ));
    }

    glm::mat4 cameraProjMatrix(const Camera3D &cam, float aspectRatio)
    {
        if (cam.projection == CameraProjection::Perspective)
        {
            return glm::perspective(
                glm::radians(cam.fovYDeg),
                aspectRatio,
                cam.nearPlane,
                cam.farPlane);
        }

        // Orthographic ---------------------------------------------------------

        if (cam.is2D)
        {
            // Extents derived from pan/zoom so the view is always centred on
            // the pan target and the zoom level controls how many world units
            // are visible.
            const float halfH = (cam.worldHeight2d / cam.zoom2d) * 0.5f;
            const float halfW = halfH * aspectRatio;
            return glm::ortho(-halfW, halfW, -halfH, halfH,
                              cam.nearPlane, cam.farPlane);
        }

        // Auto-fit: all extents are zero → compute from worldHeight2d.
        if (cam.orthoLeft == 0.f && cam.orthoRight  == 0.f &&
            cam.orthoBottom == 0.f && cam.orthoTop  == 0.f)
        {
            const float halfH = cam.worldHeight2d * 0.5f;
            const float halfW = halfH * aspectRatio;
            return glm::ortho(-halfW, halfW, -halfH, halfH,
                              cam.nearPlane, cam.farPlane);
        }

        // Explicit extents (e.g. the default BGI pixel-space camera).
        return glm::ortho(cam.orthoLeft, cam.orthoRight,
                          cam.orthoBottom, cam.orthoTop,
                          cam.nearPlane, cam.farPlane);
    }

    glm::mat4 cameraVPMatrix(const Camera3D &cam, float aspectRatio)
    {
        return cameraProjMatrix(cam, aspectRatio) * cameraViewMatrix(cam);
    }

    // -------------------------------------------------------------------------
    // Coordinate utilities
    // -------------------------------------------------------------------------

    bool cameraWorldToScreen(const Camera3D &cam, int winW, int winH,
                             float wx, float wy, float wz,
                             float &screenX, float &screenY)
    {
        int vpx, vpy, vpw, vph;
        cameraEffectiveViewport(cam, winW, winH, vpx, vpy, vpw, vph);

        if (vpw <= 0 || vph <= 0)
            return false;

        const float aspect = (vph > 0)
                           ? static_cast<float>(vpw) / static_cast<float>(vph)
                           : 1.f;

        const glm::vec4 clip = cameraVPMatrix(cam, aspect)
                             * glm::vec4(wx, wy, wz, 1.f);

        // Discard points behind the camera.
        if (clip.w <= 0.f)
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;

        // Full clip-space frustum test (X/Y/Z).
        if (ndc.x < -1.f || ndc.x > 1.f ||
            ndc.y < -1.f || ndc.y > 1.f ||
            ndc.z < -1.f || ndc.z > 1.f)
            return false;

        // NDC -> inclusive pixel indices in [vpx, vpx+vpw-1] and
        // [vpy, vpy+vph-1]. This avoids a +1 pixel spill at viewport edges.
        const float spanX = static_cast<float>(std::max(vpw - 1, 0));
        const float spanY = static_cast<float>(std::max(vph - 1, 0));

        // Y is flipped so NDC +1 maps to pixel row 0 (top), matching BGI.
        screenX = (ndc.x + 1.f) * 0.5f * spanX + static_cast<float>(vpx);
        screenY = (1.f - ndc.y) * 0.5f * spanY + static_cast<float>(vpy);

        return true;
    }

    void cameraScreenToRay(const Camera3D &cam, int winW, int winH,
                           float screenX, float screenY,
                           float &rayOx, float &rayOy, float &rayOz,
                           float &rayDx, float &rayDy, float &rayDz)
    {
        int vpx, vpy, vpw, vph;
        cameraEffectiveViewport(cam, winW, winH, vpx, vpy, vpw, vph);

        if (vpw <= 0 || vph <= 0)
            return;

        const float aspect = (vph > 0)
                           ? static_cast<float>(vpw) / static_cast<float>(vph)
                           : 1.f;

        // Screen pixel → NDC (invert the Y-flip applied in worldToScreen).
        const float spanX = static_cast<float>(std::max(vpw - 1, 0));
        const float spanY = static_cast<float>(std::max(vph - 1, 0));

        const float ndcX = (spanX > 0.f)
            ? ((screenX - static_cast<float>(vpx)) / spanX) * 2.f - 1.f
            : 0.f;
        const float ndcY = (spanY > 0.f)
            ? 1.f - ((screenY - static_cast<float>(vpy)) / spanY) * 2.f
            : 0.f;

        const glm::mat4 vpInv = glm::inverse(cameraVPMatrix(cam, aspect));

        glm::vec4 nearPt = vpInv * glm::vec4(ndcX, ndcY, -1.f, 1.f);
        glm::vec4 farPt  = vpInv * glm::vec4(ndcX, ndcY,  1.f, 1.f);

        if (nearPt.w != 0.f) nearPt /= nearPt.w;
        if (farPt.w  != 0.f) farPt  /= farPt.w;

        const glm::vec3 origin    = glm::vec3(nearPt);
        const glm::vec3 direction = glm::normalize(glm::vec3(farPt) - origin);

        rayOx = origin.x;    rayOy = origin.y;    rayOz = origin.z;
        rayDx = direction.x; rayDy = direction.y; rayDz = direction.z;
    }

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------

    void matToFloatArray(const glm::mat4 &m, float *out16)
    {
        const float *p = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i)
            out16[i] = p[i];
    }

} // namespace bgi
