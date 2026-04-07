/**
 * @file bgi_dds_scene_api.cpp
 * @brief C wrapper implementing the multi-CHDOP scene management API:
 *        wxbgi_dds_scene_*, wxbgi_cam_set_scene, wxbgi_cam_get_scene.
 *
 * All functions acquire gMutex before touching the DDS registry.
 */

#include "wx_bgi_dds.h"

#include <mutex>
#include <string>

#include "bgi_dds.h"
#include "bgi_state.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/** Resolves a scene name: NULL / empty → "default". */
static const std::string &resolveScene(const char *name)
{
    static thread_local std::string resolved;
    if (name != nullptr && name[0] != '\0')
        resolved = name;
    else
        resolved = "default";
    return resolved;
}

/** Resolves a camera name: NULL / empty → active camera. */
static const std::string &resolveCam(const char *name)
{
    static thread_local std::string resolved;
    if (name != nullptr && name[0] != '\0')
        resolved = name;
    else
        resolved = bgi::gState.activeCamera;
    return resolved;
}

/** Finds Camera3D* by resolved name; caller must hold gMutex. */
static bgi::Camera3D *findCam(const std::string &name)
{
    auto it = bgi::gState.cameras.find(name);
    return (it != bgi::gState.cameras.end()) ? &it->second->camera : nullptr;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------

BGI_API int BGI_CALL wxbgi_dds_scene_create(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return -1;
    std::string sname(name);
    if (sname == "default")
        return -1;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.ddsRegistry.count(sname))
        return -1;
    bgi::gState.ddsRegistry[sname] = std::make_unique<bgi::DdsScene>();
    return 0;
}

BGI_API void BGI_CALL wxbgi_dds_scene_destroy(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return;
    std::string sname(name);
    if (sname == "default")
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::gState.ddsRegistry.count(sname))
        return;

    bgi::gState.ddsRegistry.erase(sname);

    // If this was the active scene, fall back to "default".
    if (bgi::gState.activeDdsName == sname)
    {
        bgi::gState.activeDdsName = "default";
        bgi::gState.dds = bgi::gState.ddsRegistry["default"].get();
    }

    // Re-assign any cameras that were pointing at the destroyed scene.
    for (auto &[camName, ddsCamera] : bgi::gState.cameras)
    {
        if (ddsCamera->camera.assignedSceneName == sname)
            ddsCamera->camera.assignedSceneName = "default";
    }
}

BGI_API void BGI_CALL wxbgi_dds_scene_set_active(const char *name)
{
    const std::string &sname = resolveScene(name);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto it = bgi::gState.ddsRegistry.find(sname);
    if (it == bgi::gState.ddsRegistry.end())
        return; // scene does not exist — silently ignore
    bgi::gState.activeDdsName = sname;
    bgi::gState.dds = it->second.get();
}

BGI_API const char *BGI_CALL wxbgi_dds_scene_get_active(void)
{
    static thread_local std::string buf;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    buf = bgi::gState.activeDdsName;
    return buf.c_str();
}

BGI_API int BGI_CALL wxbgi_dds_scene_exists(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return 0;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.ddsRegistry.count(std::string(name)) ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_dds_scene_clear(const char *name)
{
    const std::string &sname = resolveScene(name);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto it = bgi::gState.ddsRegistry.find(sname);
    if (it == bgi::gState.ddsRegistry.end())
        return;
    // Remove only drawing-primitive objects; keep cameras and UCS objects.
    it->second->clearDrawingObjects();
}

// ---------------------------------------------------------------------------
// Camera-to-scene assignment
// ---------------------------------------------------------------------------

BGI_API void BGI_CALL wxbgi_cam_set_scene(const char *camName, const char *sceneName)
{
    const std::string &sname = resolveScene(sceneName);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    // Scene must exist.
    if (!bgi::gState.ddsRegistry.count(sname))
        return;
    const std::string &cname = resolveCam(camName);
    bgi::Camera3D *cam = findCam(cname);
    if (!cam)
        return;
    cam->assignedSceneName = sname;
}

BGI_API const char *BGI_CALL wxbgi_cam_get_scene(const char *camName)
{
    static thread_local std::string buf;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const std::string &cname = resolveCam(camName);
    bgi::Camera3D *cam = findCam(cname);
    if (!cam)
        return nullptr;
    buf = cam->assignedSceneName;
    return buf.c_str();
}
