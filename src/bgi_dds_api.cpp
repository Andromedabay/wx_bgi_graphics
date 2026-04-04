/**
 * @file bgi_dds_api.cpp
 * @brief C wrapper implementing the wxbgi_dds_* public API.
 *
 * Each function acquires gMutex before touching the DDS.  Return values that
 * are strings use a thread-local buffer to avoid dynamic allocation in the
 * hot path (same pattern as grapherrormsg / wxbgi_cam_get_active).
 */

#include "wx_bgi_dds.h"

#include <cmath>
#include <mutex>
#include <string>

#include "bgi_dds.h"
#include "bgi_gl.h"
#include "bgi_state.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/** Maps a DdsObjectType enum value to the corresponding WXBGI_DDS_* constant. */
static int typeToInt(bgi::DdsObjectType t)
{
    switch (t)
    {
        case bgi::DdsObjectType::Camera:          return WXBGI_DDS_CAMERA;
        case bgi::DdsObjectType::Ucs:             return WXBGI_DDS_UCS;
        case bgi::DdsObjectType::WorldExtentsObj: return WXBGI_DDS_WORLD_EXTENTS;
        case bgi::DdsObjectType::Point:           return WXBGI_DDS_POINT;
        case bgi::DdsObjectType::Line:            return WXBGI_DDS_LINE;
        case bgi::DdsObjectType::Circle:          return WXBGI_DDS_CIRCLE;
        case bgi::DdsObjectType::Arc:             return WXBGI_DDS_ARC;
        case bgi::DdsObjectType::Ellipse:         return WXBGI_DDS_ELLIPSE;
        case bgi::DdsObjectType::FillEllipse:     return WXBGI_DDS_FILL_ELLIPSE;
        case bgi::DdsObjectType::PieSlice:        return WXBGI_DDS_PIE_SLICE;
        case bgi::DdsObjectType::Sector:          return WXBGI_DDS_SECTOR;
        case bgi::DdsObjectType::Rectangle:       return WXBGI_DDS_RECTANGLE;
        case bgi::DdsObjectType::Bar:             return WXBGI_DDS_BAR;
        case bgi::DdsObjectType::Bar3D:           return WXBGI_DDS_BAR3D;
        case bgi::DdsObjectType::Polygon:         return WXBGI_DDS_POLYGON;
        case bgi::DdsObjectType::FillPoly:        return WXBGI_DDS_FILL_POLY;
        case bgi::DdsObjectType::Text:            return WXBGI_DDS_TEXT;
        case bgi::DdsObjectType::Image:           return WXBGI_DDS_IMAGE;
        case bgi::DdsObjectType::Box:             return WXBGI_DDS_BOX;
        case bgi::DdsObjectType::Sphere:          return WXBGI_DDS_SPHERE;
        case bgi::DdsObjectType::Cylinder:        return WXBGI_DDS_CYLINDER;
        case bgi::DdsObjectType::Cone:            return WXBGI_DDS_CONE;
        case bgi::DdsObjectType::Torus:           return WXBGI_DDS_TORUS;
        case bgi::DdsObjectType::HeightMap:       return WXBGI_DDS_HEIGHTMAP;
        case bgi::DdsObjectType::ParamSurface:    return WXBGI_DDS_PARAM_SURFACE;
        case bgi::DdsObjectType::Extrusion:       return WXBGI_DDS_EXTRUSION;
        default:                                  return WXBGI_DDS_UNKNOWN;
    }
}

/** Returns the CoordSpace integer for a drawing object, or -1 for config objects. */
static int coordSpaceToInt(const bgi::DdsObject &obj)
{
    // Config objects have no CoordSpace
    const auto t = obj.type;
    if (t == bgi::DdsObjectType::Camera ||
        t == bgi::DdsObjectType::Ucs    ||
        t == bgi::DdsObjectType::WorldExtentsObj)
        return -1;

    // All drawing primitives store coordSpace as their second data member.
    // We use static_cast<> for each known concrete type.
    auto getCS = [&]() -> bgi::CoordSpace {
        switch (t)
        {
            case bgi::DdsObjectType::Point:      return static_cast<const bgi::DdsPoint&>(obj).coordSpace;
            case bgi::DdsObjectType::Line:       return static_cast<const bgi::DdsLine&>(obj).coordSpace;
            case bgi::DdsObjectType::Circle:     return static_cast<const bgi::DdsCircle&>(obj).coordSpace;
            case bgi::DdsObjectType::Arc:        return static_cast<const bgi::DdsArc&>(obj).coordSpace;
            case bgi::DdsObjectType::Ellipse:    return static_cast<const bgi::DdsEllipse&>(obj).coordSpace;
            case bgi::DdsObjectType::FillEllipse:return static_cast<const bgi::DdsFillEllipse&>(obj).coordSpace;
            case bgi::DdsObjectType::PieSlice:   return static_cast<const bgi::DdsPieSlice&>(obj).coordSpace;
            case bgi::DdsObjectType::Sector:     return static_cast<const bgi::DdsSector&>(obj).coordSpace;
            case bgi::DdsObjectType::Rectangle:  return static_cast<const bgi::DdsRectangle&>(obj).coordSpace;
            case bgi::DdsObjectType::Bar:        return static_cast<const bgi::DdsBar&>(obj).coordSpace;
            case bgi::DdsObjectType::Bar3D:      return static_cast<const bgi::DdsBar3D&>(obj).coordSpace;
            case bgi::DdsObjectType::Polygon:    return static_cast<const bgi::DdsPolygon&>(obj).coordSpace;
            case bgi::DdsObjectType::FillPoly:   return static_cast<const bgi::DdsFillPoly&>(obj).coordSpace;
            case bgi::DdsObjectType::Text:       return static_cast<const bgi::DdsText&>(obj).coordSpace;
            case bgi::DdsObjectType::Image:      return bgi::CoordSpace::BgiPixel; // images always pixel-space
            // Phase 4/5/6 solid types — coordSpace is in DdsSolid3D base
            case bgi::DdsObjectType::Box:
            case bgi::DdsObjectType::Sphere:
            case bgi::DdsObjectType::Cylinder:
            case bgi::DdsObjectType::Cone:
            case bgi::DdsObjectType::Torus:
            case bgi::DdsObjectType::HeightMap:
            case bgi::DdsObjectType::ParamSurface:
            case bgi::DdsObjectType::Extrusion:
                return static_cast<const bgi::DdsSolid3D&>(obj).coordSpace;
            default:                             return bgi::CoordSpace::BgiPixel;
        }
    };

    switch (getCS())
    {
        case bgi::CoordSpace::BgiPixel: return WXBGI_COORD_BGI_PIXEL;
        case bgi::CoordSpace::World3D:  return WXBGI_COORD_WORLD_3D;
        case bgi::CoordSpace::UcsLocal: return WXBGI_COORD_UCS_LOCAL;
        default:                        return WXBGI_COORD_BGI_PIXEL;
    }
}

} // anonymous namespace

// =============================================================================
// wxbgi_dds_object_count
// =============================================================================

BGI_API int BGI_CALL wxbgi_dds_object_count(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.dds->count();
}

// =============================================================================
// wxbgi_dds_get_id_at
// =============================================================================

BGI_API const char *BGI_CALL wxbgi_dds_get_id_at(int index)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    static thread_local std::string result;
    result = bgi::gState.dds->idAt(index);
    return result.c_str();
}

// =============================================================================
// wxbgi_dds_get_type
// =============================================================================

BGI_API int BGI_CALL wxbgi_dds_get_type(const char *id)
{
    if (id == nullptr || id[0] == '\0')
        return WXBGI_DDS_UNKNOWN;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    return obj ? typeToInt(obj->type) : WXBGI_DDS_UNKNOWN;
}

// =============================================================================
// wxbgi_dds_get_coord_space
// =============================================================================

BGI_API int BGI_CALL wxbgi_dds_get_coord_space(const char *id)
{
    if (id == nullptr || id[0] == '\0')
        return -1;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    if (!obj)
        return -1;
    return coordSpaceToInt(*obj);
}

// =============================================================================
// wxbgi_dds_get_label
// =============================================================================

BGI_API const char *BGI_CALL wxbgi_dds_get_label(const char *id)
{
    static thread_local std::string result;
    result.clear();

    if (id == nullptr || id[0] == '\0')
        return result.c_str();

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    if (obj)
        result = obj->label;
    return result.c_str();
}

// =============================================================================
// wxbgi_dds_set_label
// =============================================================================

BGI_API void BGI_CALL wxbgi_dds_set_label(const char *id, const char *label)
{
    if (id == nullptr || id[0] == '\0')
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    if (obj)
        obj->label = (label != nullptr) ? label : "";
}

// =============================================================================
// wxbgi_dds_find_by_label
// =============================================================================

BGI_API const char *BGI_CALL wxbgi_dds_find_by_label(const char *label)
{
    static thread_local std::string result;
    result.clear();

    if (label == nullptr || label[0] == '\0')
        return result.c_str();

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    result = bgi::gState.dds->findIdByLabel(label);
    return result.c_str();
}

// =============================================================================
// wxbgi_dds_set_visible / wxbgi_dds_get_visible
// =============================================================================

BGI_API void BGI_CALL wxbgi_dds_set_visible(const char *id, int visible)
{
    if (id == nullptr || id[0] == '\0')
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    if (obj)
        obj->visible = (visible != 0);
}

BGI_API int BGI_CALL wxbgi_dds_get_visible(const char *id)
{
    if (id == nullptr || id[0] == '\0')
        return 0;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    auto obj = bgi::gState.dds->findById(id);
    return (obj && obj->visible) ? 1 : 0;
}

// =============================================================================
// wxbgi_dds_remove
// =============================================================================

BGI_API int BGI_CALL wxbgi_dds_remove(const char *id)
{
    if (id == nullptr || id[0] == '\0')
        return 0;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    // If the object is a camera or UCS, also remove from the BgiState maps.
    auto obj = bgi::gState.dds->findById(id);
    if (obj)
    {
        if (obj->type == bgi::DdsObjectType::Camera)
        {
            const auto &camName = static_cast<bgi::DdsCamera &>(*obj).name;
            if (camName != "default")
                bgi::gState.cameras.erase(camName);
        }
        else if (obj->type == bgi::DdsObjectType::Ucs)
        {
            const auto &ucsName = static_cast<bgi::DdsUcs &>(*obj).name;
            if (ucsName != "world")
                bgi::gState.ucsSystems.erase(ucsName);
        }
    }

    return bgi::gState.dds->remove(id) ? 1 : 0;
}

// =============================================================================
// wxbgi_dds_clear
// =============================================================================

BGI_API void BGI_CALL wxbgi_dds_clear(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.dds->clearDrawingObjects();
}

// =============================================================================
// wxbgi_dds_clear_all
// =============================================================================

BGI_API void BGI_CALL wxbgi_dds_clear_all(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    bgi::gState.dds->clearAll();
    bgi::gState.cameras.clear();
    bgi::gState.ucsSystems.clear();
    bgi::gState.worldExtents = bgi::WorldExtents{};
    bgi::gState.activeCamera = "default";
    bgi::gState.activeUcs    = "world";

    // Recreate the mandatory default camera.
    {
        auto dc = std::make_shared<bgi::DdsCamera>();
        dc->name               = "default";
        dc->camera.projection  = bgi::CameraProjection::Orthographic;
        dc->camera.eyeX        = 0.f; dc->camera.eyeY = 0.f; dc->camera.eyeZ = 1.f;
        dc->camera.targetX     = 0.f; dc->camera.targetY = 0.f; dc->camera.targetZ = 0.f;
        dc->camera.upX         = 0.f; dc->camera.upY = 1.f; dc->camera.upZ = 0.f;
        dc->camera.orthoLeft   = 0.f;
        dc->camera.orthoRight  = static_cast<float>(bgi::gState.width);
        dc->camera.orthoBottom = static_cast<float>(bgi::gState.height);
        dc->camera.orthoTop    = 0.f;
        dc->camera.nearPlane   = -1.f;
        dc->camera.farPlane    =  1.f;
        bgi::gState.dds->append(dc);
        bgi::gState.cameras["default"] = dc;
    }

    // Recreate the mandatory world UCS.
    {
        auto du = std::make_shared<bgi::DdsUcs>();
        du->name = "world";
        du->ucs  = bgi::CoordSystem{};
        bgi::gState.dds->append(du);
        bgi::gState.ucsSystems["world"] = du;
    }
}

// =============================================================================
// wxbgi_render_dds  — implemented in bgi_dds_render.cpp (Phase D)
// =============================================================================
// The BGI_API definition lives in bgi_dds_render.cpp; do not define a stub
// here to avoid a duplicate-symbol linker error.

// =============================================================================
// GL rendering mode + lighting API
// =============================================================================

BGI_API void BGI_CALL wxbgi_set_legacy_gl_render(int enable)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.legacyGlRender = (enable != 0);
}

BGI_API void BGI_CALL wxbgi_gl_pass_destroy(void)
{
    // Must be called with the GL context current and bgi::gMutex NOT held
    // (called from WxBgiCanvas destructor, outside any BGI API call).
    bgi::glPassDestroy();
}

BGI_API void BGI_CALL wxbgi_solid_set_light_dir(float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) { x /= len; y /= len; z /= len; }
    bgi::gState.lightState.dirX = x;
    bgi::gState.lightState.dirY = y;
    bgi::gState.lightState.dirZ = z;
}

BGI_API void BGI_CALL wxbgi_solid_set_light_space(int worldSpace)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.lightState.worldSpace = (worldSpace != 0);
}

BGI_API void BGI_CALL wxbgi_solid_set_fill_light(float x, float y, float z, float strength)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) { x /= len; y /= len; z /= len; }
    bgi::gState.lightState.fillX       = x;
    bgi::gState.lightState.fillY       = y;
    bgi::gState.lightState.fillZ       = z;
    bgi::gState.lightState.fillStrength = strength;
}

BGI_API void BGI_CALL wxbgi_solid_set_ambient(float a)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.lightState.ambient = a;
}

BGI_API void BGI_CALL wxbgi_solid_set_diffuse(float d)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.lightState.diffuse = d;
}

BGI_API void BGI_CALL wxbgi_solid_set_specular(float s, float shininess)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.lightState.specular  = s;
    bgi::gState.lightState.shininess = shininess;
}
