#include "wx_bgi_3d.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_state.h"
#include "bgi_ucs.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

    static const std::string &resolveUcsName(const char *name)
    {
        static thread_local std::string resolved;
        if (name != nullptr && name[0] != '\0')
            resolved = name;
        else
            resolved = bgi::gState.activeUcs;
        return resolved;
    }

    static bgi::CoordSystem *findUcs(const std::string &name)
    {
        auto it = bgi::gState.ucsSystems.find(name);
        return (it != bgi::gState.ucsSystems.end()) ? &it->second->ucs : nullptr;
    }

    // Camera name resolver (duplicated from bgi_camera_api to avoid cross-TU
    // static state sharing issues).
    static const std::string &resolveCamName(const char *name)
    {
        static thread_local std::string resolved;
        if (name != nullptr && name[0] != '\0')
            resolved = name;
        else
            resolved = bgi::gState.activeCamera;
        return resolved;
    }

    static bgi::Camera3D *findCamera(const std::string &name)
    {
        auto it = bgi::gState.cameras.find(name);
        return (it != bgi::gState.cameras.end()) ? &it->second->camera : nullptr;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// UCS lifecycle
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_ucs_create(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return 0;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    if (bgi::gState.window == nullptr)
        return -1;

    auto du = std::make_shared<bgi::DdsUcs>();
    du->name = name;
    du->ucs  = bgi::CoordSystem{};

    bgi::gState.dds->append(du);
    bgi::gState.ucsSystems[name] = du;
    return 1;
}

BGI_API void BGI_CALL wxbgi_ucs_destroy(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return;
    if (std::string_view(name) == "world")
        return; // "world" UCS is protected

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    auto it = bgi::gState.ucsSystems.find(name);
    if (it != bgi::gState.ucsSystems.end())
    {
        it->second->deleted = true;
        bgi::gState.ucsSystems.erase(it);
    }
}

BGI_API void BGI_CALL wxbgi_ucs_set_active(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.ucsSystems.count(name))
        bgi::gState.activeUcs = name;
}

BGI_API const char *BGI_CALL wxbgi_ucs_get_active(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    static thread_local std::string result;
    result = bgi::gState.activeUcs;
    return result.c_str();
}

// ---------------------------------------------------------------------------
// UCS origin
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_ucs_set_origin(const char *name,
                                            float ox, float oy, float oz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cs = findUcs(resolveUcsName(name)))
    {
        cs->originX = ox;
        cs->originY = oy;
        cs->originZ = oz;
    }
}

BGI_API void BGI_CALL wxbgi_ucs_get_origin(const char *name,
                                            float *ox, float *oy, float *oz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cs = findUcs(resolveUcsName(name)))
    {
        if (ox) *ox = cs->originX;
        if (oy) *oy = cs->originY;
        if (oz) *oz = cs->originZ;
    }
}

// ---------------------------------------------------------------------------
// UCS axes
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_ucs_set_axes(const char *name,
                                          float xx, float xy, float xz,
                                          float yx, float yy, float yz,
                                          float zx, float zy, float zz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto *cs = findUcs(resolveUcsName(name));
    if (cs == nullptr)
        return;

    cs->xAxisX = xx; cs->xAxisY = xy; cs->xAxisZ = xz;
    cs->yAxisX = yx; cs->yAxisY = yy; cs->yAxisZ = yz;
    cs->zAxisX = zx; cs->zAxisY = zy; cs->zAxisZ = zz;
    bgi::ucsOrthonormalise(*cs);
}

BGI_API void BGI_CALL wxbgi_ucs_get_axes(const char *name,
                                          float *xx, float *xy, float *xz,
                                          float *yx, float *yy, float *yz,
                                          float *zx, float *zy, float *zz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const auto *cs = findUcs(resolveUcsName(name));
    if (cs == nullptr)
        return;

    if (xx) *xx = cs->xAxisX; if (xy) *xy = cs->xAxisY; if (xz) *xz = cs->xAxisZ;
    if (yx) *yx = cs->yAxisX; if (yy) *yy = cs->yAxisY; if (yz) *yz = cs->yAxisZ;
    if (zx) *zx = cs->zAxisX; if (zy) *zy = cs->zAxisY; if (zz) *zz = cs->zAxisZ;
}

BGI_API void BGI_CALL wxbgi_ucs_align_to_plane(const char *name,
                                                float nx, float ny, float nz,
                                                float ox, float oy, float oz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto *cs = findUcs(resolveUcsName(name));
    if (cs == nullptr)
        return;

    *cs = bgi::ucsFromNormal(nx, ny, nz, ox, oy, oz);
}

// ---------------------------------------------------------------------------
// UCS matrix access
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_ucs_get_local_to_world_matrix(const char *name,
                                                           float *out16)
{
    if (out16 == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cs = findUcs(resolveUcsName(name)))
        bgi::matToFloatArray(bgi::ucsLocalToWorldMatrix(*cs), out16);
}

BGI_API void BGI_CALL wxbgi_ucs_get_world_to_local_matrix(const char *name,
                                                           float *out16)
{
    if (out16 == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cs = findUcs(resolveUcsName(name)))
        bgi::matToFloatArray(bgi::ucsWorldToLocalMatrix(*cs), out16);
}

// ---------------------------------------------------------------------------
// UCS coordinate transforms
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_ucs_world_to_local(const char *name,
                                                float wx, float wy, float wz,
                                                float *lx, float *ly, float *lz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const auto *cs = findUcs(resolveUcsName(name));
    if (cs == nullptr)
        return;

    float x = 0.f, y = 0.f, z = 0.f;
    bgi::ucsWorldToLocal(*cs, wx, wy, wz, x, y, z);
    if (lx) *lx = x;
    if (ly) *ly = y;
    if (lz) *lz = z;
}

BGI_API void BGI_CALL wxbgi_ucs_local_to_world(const char *name,
                                                float lx, float ly, float lz,
                                                float *wx, float *wy, float *wz)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const auto *cs = findUcs(resolveUcsName(name));
    if (cs == nullptr)
        return;

    float x = 0.f, y = 0.f, z = 0.f;
    bgi::ucsLocalToWorld(*cs, lx, ly, lz, x, y, z);
    if (wx) *wx = x;
    if (wy) *wy = y;
    if (wz) *wz = z;
}

// ---------------------------------------------------------------------------
// World extents
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_set_world_extents(float minX, float minY, float minZ,
                                               float maxX, float maxY, float maxZ)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.worldExtents.minX    = minX;
    bgi::gState.worldExtents.minY    = minY;
    bgi::gState.worldExtents.minZ    = minZ;
    bgi::gState.worldExtents.maxX    = maxX;
    bgi::gState.worldExtents.maxY    = maxY;
    bgi::gState.worldExtents.maxZ    = maxZ;
    bgi::gState.worldExtents.hasData = true;
}

BGI_API int BGI_CALL wxbgi_get_world_extents(float *minX, float *minY, float *minZ,
                                              float *maxX, float *maxY, float *maxZ)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const auto &e = bgi::gState.worldExtents;
    if (minX) *minX = e.minX;
    if (minY) *minY = e.minY;
    if (minZ) *minZ = e.minZ;
    if (maxX) *maxX = e.maxX;
    if (maxY) *maxY = e.maxY;
    if (maxZ) *maxZ = e.maxZ;
    return e.hasData ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_expand_world_extents(float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::worldExtentsExpand(bgi::gState.worldExtents, x, y, z);
}

BGI_API void BGI_CALL wxbgi_reset_world_extents(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.worldExtents = bgi::WorldExtents{};
}

// ---------------------------------------------------------------------------
// wxbgi_cam_fit_to_extents
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_cam_fit_to_extents(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto &extents = bgi::gState.worldExtents;
    if (!extents.hasData)
        return 0;

    auto *cam = findCamera(resolveCamName(camName));
    if (cam == nullptr)
        return -1;

    const glm::vec3 centre   = bgi::worldExtentsCentre(extents);
    const glm::vec3 halfSize = bgi::worldExtentsHalfSize(extents);

    // Bounding sphere radius.
    const float radius = glm::length(halfSize);

    const float aspect = bgi::cameraAspectRatio(
        *cam, bgi::gState.width, bgi::gState.height);

    constexpr float kPad = 1.05f; // 5 % padding

    if (cam->projection == bgi::CameraProjection::Orthographic)
    {
        if (cam->is2D)
        {
            // Centre pan on AABB and adjust zoom so the full extent fits.
            cam->pan2dX = centre.x;
            cam->pan2dY = centre.y;

            // Visible height needed to contain the larger of the two dimensions.
            const float neededH = std::max(halfSize.y * 2.f,
                                           halfSize.x * 2.f / aspect) * kPad;
            cam->zoom2d = cam->worldHeight2d / neededH;
        }
        else
        {
            // Set explicit ortho extents centred on the AABB.
            const float halfH = std::max(halfSize.y, halfSize.x / aspect) * kPad;
            const float halfW = halfH * aspect;
            cam->orthoLeft   = centre.x - halfW;
            cam->orthoRight  = centre.x + halfW;
            cam->orthoBottom = centre.y - halfH;
            cam->orthoTop    = centre.y + halfH;
            // Clear auto-fit sentinel so our explicit values are used.
            cam->worldHeight2d = halfH * 2.f;
        }
    }
    else // Perspective
    {
        // Move the eye along the current view direction so the bounding sphere
        // fits within the vertical half-FOV.
        const float halfFovY  = glm::radians(cam->fovYDeg * 0.5f);
        const float halfFovX  = std::atan(std::tan(halfFovY) * aspect);
        const float minFov    = std::min(halfFovY, halfFovX);
        const float dist      = (minFov > 0.f)
                              ? radius * kPad / std::tan(minFov)
                              : radius * kPad * 2.f;

        const glm::vec3 eye(cam->eyeX, cam->eyeY, cam->eyeZ);
        const glm::vec3 tgt(cam->targetX, cam->targetY, cam->targetZ);
        const glm::vec3 dir = glm::length(eye - tgt) > 1e-6f
                            ? glm::normalize(eye - tgt)
                            : glm::vec3(0.f, 0.f, 1.f);

        const glm::vec3 newEye = centre + dir * dist;
        cam->eyeX    = newEye.x;
        cam->eyeY    = newEye.y;
        cam->eyeZ    = newEye.z;
        cam->targetX = centre.x;
        cam->targetY = centre.y;
        cam->targetZ = centre.z;
    }

    return 1;
}
