#include "wx_bgi_3d.h"

#include <algorithm>
#include <mutex>
#include <string>

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_state.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

    /** Resolves a camera name: NULL means "active camera". */
    static const std::string &resolveName(const char *name)
    {
        static thread_local std::string resolved;
        if (name != nullptr && name[0] != '\0')
            resolved = name;
        else
            resolved = bgi::gState.activeCamera;
        return resolved;
    }

    /**
     * Looks up a camera by name (already resolved).  Returns a raw pointer to
     * the Camera3D data inside the DdsCamera, or nullptr if not found.
     * Caller must hold gMutex.
     */
    static bgi::Camera3D *findCamera(const std::string &name)
    {
        auto it = bgi::gState.cameras.find(name);
        return (it != bgi::gState.cameras.end()) ? &it->second->camera : nullptr;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Camera lifecycle
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_cam_create(const char *name, int type)
{
    if (name == nullptr || name[0] == '\0')
        return 0;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (bgi::gState.window == nullptr)
        return -1;

    auto dc = std::make_shared<bgi::DdsCamera>();
    dc->name            = name;
    dc->camera.projection = (type == WXBGI_CAM_PERSPECTIVE)
                           ? bgi::CameraProjection::Perspective
                           : bgi::CameraProjection::Orthographic;

    bgi::gState.dds->append(dc);
    bgi::gState.cameras[name] = dc;
    return 1;
}

BGI_API void BGI_CALL wxbgi_cam_destroy(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return;
    // The "default" camera must not be removed.
    if (std::string_view(name) == "default")
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    // Remove from DDS (soft-delete by marking the DdsCamera object deleted).
    auto it = bgi::gState.cameras.find(name);
    if (it != bgi::gState.cameras.end())
    {
        it->second->deleted = true;
        bgi::gState.cameras.erase(it);
    }
}

BGI_API void BGI_CALL wxbgi_cam_set_active(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.cameras.count(name))
        bgi::gState.activeCamera = name;
}

BGI_API const char *BGI_CALL wxbgi_cam_get_active(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    static thread_local std::string result;
    result = bgi::gState.activeCamera;
    return result.c_str();
}

// ---------------------------------------------------------------------------
// Eye / target / up
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_cam_set_eye(const char *name,
                                         float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->eyeX = x;
        cam->eyeY = y;
        cam->eyeZ = z;
    }
}

BGI_API void BGI_CALL wxbgi_cam_set_target(const char *name,
                                            float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->targetX = x;
        cam->targetY = y;
        cam->targetZ = z;
    }
}

BGI_API void BGI_CALL wxbgi_cam_set_up(const char *name,
                                        float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->upX = x;
        cam->upY = y;
        cam->upZ = z;
    }
}

BGI_API void BGI_CALL wxbgi_cam_get_eye(const char *name,
                                         float *x, float *y, float *z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        if (x) *x = cam->eyeX;
        if (y) *y = cam->eyeY;
        if (z) *z = cam->eyeZ;
    }
}

BGI_API void BGI_CALL wxbgi_cam_get_target(const char *name,
                                            float *x, float *y, float *z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        if (x) *x = cam->targetX;
        if (y) *y = cam->targetY;
        if (z) *z = cam->targetZ;
    }
}

BGI_API void BGI_CALL wxbgi_cam_get_up(const char *name,
                                        float *x, float *y, float *z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        if (x) *x = cam->upX;
        if (y) *y = cam->upY;
        if (z) *z = cam->upZ;
    }
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_cam_set_perspective(const char *name,
                                                 float fovYDeg,
                                                 float nearPlane,
                                                 float farPlane)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->projection = bgi::CameraProjection::Perspective;
        cam->fovYDeg    = fovYDeg;
        cam->nearPlane  = nearPlane;
        cam->farPlane   = farPlane;
    }
}

BGI_API void BGI_CALL wxbgi_cam_set_ortho(const char *name,
                                           float left, float right,
                                           float bottom, float top,
                                           float nearPlane, float farPlane)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->projection   = bgi::CameraProjection::Orthographic;
        cam->orthoLeft    = left;
        cam->orthoRight   = right;
        cam->orthoBottom  = bottom;
        cam->orthoTop     = top;
        cam->nearPlane    = nearPlane;
        cam->farPlane     = farPlane;
    }
}

BGI_API void BGI_CALL wxbgi_cam_set_ortho_auto(const char *name,
                                                float worldUnitsHigh,
                                                float nearPlane,
                                                float farPlane)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->projection   = bgi::CameraProjection::Orthographic;
        cam->orthoLeft    = 0.f;
        cam->orthoRight   = 0.f;
        cam->orthoBottom  = 0.f;
        cam->orthoTop     = 0.f;
        cam->worldHeight2d = worldUnitsHigh;
        cam->nearPlane    = nearPlane;
        cam->farPlane     = farPlane;
    }
}

// ---------------------------------------------------------------------------
// Screen viewport
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_cam_set_screen_viewport(const char *name,
                                                     int x, int y,
                                                     int w, int h)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->vpX = x;
        cam->vpY = y;
        cam->vpW = w;
        cam->vpH = h;
    }
}

// ---------------------------------------------------------------------------
// Matrix access
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_cam_get_view_matrix(const char *name, float *out16)
{
    if (out16 == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        const float aspect = bgi::cameraAspectRatio(
            *cam, bgi::gState.width, bgi::gState.height);
        bgi::matToFloatArray(bgi::cameraViewMatrix(*cam), out16);
    }
}

BGI_API void BGI_CALL wxbgi_cam_get_proj_matrix(const char *name, float *out16)
{
    if (out16 == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        const float aspect = bgi::cameraAspectRatio(
            *cam, bgi::gState.width, bgi::gState.height);
        bgi::matToFloatArray(bgi::cameraProjMatrix(*cam, aspect), out16);
    }
}

BGI_API void BGI_CALL wxbgi_cam_get_vp_matrix(const char *name, float *out16)
{
    if (out16 == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        const float aspect = bgi::cameraAspectRatio(
            *cam, bgi::gState.width, bgi::gState.height);
        bgi::matToFloatArray(bgi::cameraVPMatrix(*cam, aspect), out16);
    }
}

// ---------------------------------------------------------------------------
// Coordinate utilities
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_cam_world_to_screen(const char *name,
                                                float wx, float wy, float wz,
                                                float *screenX, float *screenY)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (bgi::gState.window == nullptr)
        return -1;

    const auto *cam = findCamera(resolveName(name));
    if (cam == nullptr)
        return -1;

    float sx = 0.f, sy = 0.f;
    const bool visible = bgi::cameraWorldToScreen(
        *cam, bgi::gState.width, bgi::gState.height, wx, wy, wz, sx, sy);

    if (screenX) *screenX = sx;
    if (screenY) *screenY = sy;
    return visible ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_cam_screen_to_ray(const char *name,
                                               float screenX, float screenY,
                                               float *rayOx, float *rayOy, float *rayOz,
                                               float *rayDx, float *rayDy, float *rayDz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCamera(resolveName(name));
    if (cam == nullptr)
        return;

    float ox = 0.f, oy = 0.f, oz = 0.f;
    float dx = 0.f, dy = 0.f, dz = 0.f;

    bgi::cameraScreenToRay(*cam, bgi::gState.width, bgi::gState.height,
                            screenX, screenY,
                            ox, oy, oz, dx, dy, dz);

    if (rayOx) *rayOx = ox;
    if (rayOy) *rayOy = oy;
    if (rayOz) *rayOz = oz;
    if (rayDx) *rayDx = dx;
    if (rayDy) *rayDy = dy;
    if (rayDz) *rayDz = dz;
}

// ---------------------------------------------------------------------------
// 2-D camera convenience API
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_cam2d_create(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return 0;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (bgi::gState.window == nullptr)
        return -1;

    auto dc = std::make_shared<bgi::DdsCamera>();
    dc->name                  = name;
    dc->camera.is2D           = true;
    dc->camera.projection     = bgi::CameraProjection::Orthographic;
    dc->camera.pan2dX         = 0.f;
    dc->camera.pan2dY         = 0.f;
    dc->camera.zoom2d         = 1.f;
    dc->camera.rot2dDeg       = 0.f;
    dc->camera.worldHeight2d  = static_cast<float>(bgi::gState.height);
    // Wide Z range so 3-D objects above/below the XY plane remain visible.
    dc->camera.nearPlane      = -10000.f;
    dc->camera.farPlane       =  10000.f;

    bgi::gState.dds->append(dc);
    bgi::gState.cameras[name] = dc;
    return 1;
}

BGI_API void BGI_CALL wxbgi_cam2d_set_pan(const char *name, float x, float y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->pan2dX = x;
        cam->pan2dY = y;
    }
}

BGI_API void BGI_CALL wxbgi_cam2d_pan_by(const char *name, float dx, float dy)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
    {
        cam->pan2dX += dx;
        cam->pan2dY += dy;
    }
}

BGI_API void BGI_CALL wxbgi_cam2d_set_zoom(const char *name, float zoom)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
        cam->zoom2d = std::max(1e-6f, zoom);
}

BGI_API void BGI_CALL wxbgi_cam2d_zoom_at(const char *name,
                                           float factor,
                                           float pivotX, float pivotY)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto *cam = findCamera(resolveName(name));
    if (cam == nullptr)
        return;

    const float oldZoom = cam->zoom2d;
    const float newZoom = std::max(1e-6f, oldZoom * factor);

    // Keep the world point under the pivot stationary.
    // pan_new = pivot + (pan_old - pivot) * (oldZoom / newZoom)
    const float ratio = oldZoom / newZoom;
    cam->pan2dX  = pivotX + (cam->pan2dX - pivotX) * ratio;
    cam->pan2dY  = pivotY + (cam->pan2dY - pivotY) * ratio;
    cam->zoom2d  = newZoom;
}

BGI_API void BGI_CALL wxbgi_cam2d_set_rotation(const char *name, float angleDeg)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
        cam->rot2dDeg = angleDeg;
}

BGI_API void BGI_CALL wxbgi_cam2d_get_pan(const char *name,
                                           float *x, float *y)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
    {
        if (x) *x = cam->pan2dX;
        if (y) *y = cam->pan2dY;
    }
}

BGI_API void BGI_CALL wxbgi_cam2d_get_zoom(const char *name, float *zoom)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
        if (zoom) *zoom = cam->zoom2d;
}

BGI_API void BGI_CALL wxbgi_cam2d_get_rotation(const char *name, float *angleDeg)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCamera(resolveName(name)))
        if (angleDeg) *angleDeg = cam->rot2dDeg;
}

BGI_API void BGI_CALL wxbgi_cam2d_set_world_height(const char *name,
                                                    float worldUnitsHigh)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCamera(resolveName(name)))
        cam->worldHeight2d = std::max(1e-6f, worldUnitsHigh);
}
