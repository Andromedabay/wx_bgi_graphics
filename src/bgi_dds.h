#pragma once

/**
 * @file bgi_dds.h
 * @brief Internal CHDOP (Class Hierarchy of Drawing Object Primitives) and
 *        DdsScene container for the Drawing Description Data Structure (DDS).
 *
 * The DDS is an in-memory, retained-mode scene graph.  Every draw call in the
 * library creates a DdsObject derived class in addition to rendering immediately
 * to the pixel buffer (hybrid retained + immediate mode).
 *
 * Key design properties:
 *  - Sequential access: DdsScene::order vector preserves insertion order.
 *  - O(1) direct access: DdsScene::index unordered_map keys by auto-assigned ID.
 *  - World-space coordinates: ALL drawing objects store floating-point world
 *    coordinates.  Classic BGI pixel integers are recorded as (x, y, Z=0) with
 *    CoordSpace::BgiPixel tag.  Through the default pixel-space ortho camera
 *    they project identically; through any other camera they reproject correctly.
 *  - DDS is the source of truth for cameras and UCS:
 *    BgiState::cameras holds shared_ptr<DdsCamera> — the Camera3D data lives
 *    inside the DdsCamera object.  Modifying a camera via wxbgi_cam_set_eye()
 *    updates the DdsCamera in-place with no sync step.
 *
 * This header is internal — not part of the public API.
 */

#include "bgi_types.h"  // Camera3D, CoordSystem, WorldExtents, linesettingstype, …

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace bgi
{

// =============================================================================
// Coordinate-space tag
// =============================================================================

/**
 * Indicates the coordinate space in which a drawing object's coordinates are
 * expressed when it was recorded.  The DDS-based renderer uses this to choose
 * the correct projection pipeline.
 */
enum class CoordSpace
{
    BgiPixel, ///< Pixel integers from classic BGI, stored as (x, y, 0) world coords.
    World3D,  ///< World-space 3-D coordinates (from wxbgi_world_* functions).
    UcsLocal, ///< UCS-local coordinates; ucsName identifies the frame.
};

// =============================================================================
// Per-object style snapshot
// =============================================================================

/**
 * @brief Baked rendering state captured at draw-call time.
 *
 * State-change functions (setcolor, setlinestyle, etc.) are not recorded as
 * DDS objects.  Instead their effect is baked into the next drawing object
 * that is appended to the scene.
 */
struct DdsStyle
{
    int              color{0};
    linesettingstype lineStyle{};
    fillsettingstype fillStyle{};
    textsettingstype textStyle{};
    int              writeMode{0};
    int              bkColor{0};
};

// =============================================================================
// Object type discriminator
// =============================================================================

enum class DdsObjectType
{
    // Scene-configuration objects ─────────────────────────────────────────────
    Camera,
    Ucs,
    WorldExtentsObj,

    // Drawing-primitive objects ───────────────────────────────────────────────
    Point,
    Line,
    Circle,
    Arc,
    Ellipse,
    FillEllipse,
    PieSlice,
    Sector,
    Rectangle,
    Bar,
    Bar3D,
    Polygon,
    FillPoly,
    Text,
    Image,
};

// =============================================================================
// Base class
// =============================================================================

class DdsObject
{
public:
    std::string   id;             ///< Auto-generated sequential key ("1", "2", …).
    std::string   label;          ///< Optional user label; empty = unlabelled.
    DdsObjectType type{DdsObjectType::Point};
    bool          visible{true};
    bool          deleted{false}; ///< Soft-delete; object stays in index.
    DdsStyle      style;          ///< Baked draw state at creation time.

    virtual ~DdsObject() = default;
};

// =============================================================================
// Scene-configuration objects (DDS is the source of truth)
// =============================================================================

/**
 * @brief Camera stored in the DDS.
 *
 * BgiState::cameras maps name → shared_ptr<DdsCamera>.  The Camera3D data
 * lives here; there is no separate copy in BgiState.
 */
class DdsCamera : public DdsObject
{
public:
    std::string name;   ///< Registry key in BgiState::cameras.
    Camera3D    camera; ///< Full camera data.

    DdsCamera() { type = DdsObjectType::Camera; }
};

/**
 * @brief UCS stored in the DDS.
 *
 * BgiState::ucsSystems maps name → shared_ptr<DdsUcs>.  The CoordSystem data
 * lives here; there is no separate copy in BgiState.
 */
class DdsUcs : public DdsObject
{
public:
    std::string name; ///< Registry key in BgiState::ucsSystems.
    CoordSystem ucs;  ///< Full UCS data.

    DdsUcs() { type = DdsObjectType::Ucs; }
};

/** @brief World-extents AABB stored in the DDS. */
class DdsWorldExtentsObj : public DdsObject
{
public:
    WorldExtents extents;

    DdsWorldExtentsObj() { type = DdsObjectType::WorldExtentsObj; }
};

// =============================================================================
// Drawing-primitive objects
// =============================================================================

// All positions are stored as world-space glm::vec3.
// CoordSpace::BgiPixel objects record pixel integers cast to float with Z=0.

struct DdsPoint : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   pos{0.f, 0.f, 0.f};
    int         color{0};

    DdsPoint() { type = DdsObjectType::Point; }
};

struct DdsLine : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   p1{0.f, 0.f, 0.f};
    glm::vec3   p2{0.f, 0.f, 0.f};

    DdsLine() { type = DdsObjectType::Line; }
};

struct DdsCircle : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       radius{0.f};

    DdsCircle() { type = DdsObjectType::Circle; }
};

struct DdsArc : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       radius{0.f};
    float       startAngle{0.f};
    float       endAngle{0.f};

    DdsArc() { type = DdsObjectType::Arc; }
};

struct DdsEllipse : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       rx{0.f};
    float       ry{0.f};
    float       startAngle{0.f};
    float       endAngle{0.f};

    DdsEllipse() { type = DdsObjectType::Ellipse; }
};

struct DdsFillEllipse : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       rx{0.f};
    float       ry{0.f};

    DdsFillEllipse() { type = DdsObjectType::FillEllipse; }
};

struct DdsPieSlice : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       radius{0.f};
    float       startAngle{0.f};
    float       endAngle{0.f};

    DdsPieSlice() { type = DdsObjectType::PieSlice; }
};

struct DdsSector : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   centre{0.f, 0.f, 0.f};
    float       rx{0.f};
    float       ry{0.f};
    float       startAngle{0.f};
    float       endAngle{0.f};

    DdsSector() { type = DdsObjectType::Sector; }
};

struct DdsRectangle : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   p1{0.f, 0.f, 0.f};
    glm::vec3   p2{0.f, 0.f, 0.f};

    DdsRectangle() { type = DdsObjectType::Rectangle; }
};

struct DdsBar : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   p1{0.f, 0.f, 0.f};
    glm::vec3   p2{0.f, 0.f, 0.f};

    DdsBar() { type = DdsObjectType::Bar; }
};

struct DdsBar3D : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   p1{0.f, 0.f, 0.f};
    glm::vec3   p2{0.f, 0.f, 0.f};
    float       depth{0.f};
    bool        topFlag{true};

    DdsBar3D() { type = DdsObjectType::Bar3D; }
};

struct DdsPolygon : public DdsObject
{
    CoordSpace             coordSpace{CoordSpace::BgiPixel};
    std::string            ucsName;
    std::vector<glm::vec3> pts;

    DdsPolygon() { type = DdsObjectType::Polygon; }
};

struct DdsFillPoly : public DdsObject
{
    CoordSpace             coordSpace{CoordSpace::BgiPixel};
    std::string            ucsName;
    std::vector<glm::vec3> pts;

    DdsFillPoly() { type = DdsObjectType::FillPoly; }
};

struct DdsText : public DdsObject
{
    CoordSpace  coordSpace{CoordSpace::BgiPixel};
    std::string ucsName;
    glm::vec3   pos{0.f, 0.f, 0.f};
    std::string text;

    DdsText() { type = DdsObjectType::Text; }
};

struct DdsImage : public DdsObject
{
    CoordSpace           coordSpace{CoordSpace::BgiPixel};
    glm::vec3            pos{0.f, 0.f, 0.f};
    int                  width{0};
    int                  height{0};
    std::vector<uint8_t> pixels; ///< Raw BGI palette-indexed pixels (width × height bytes).

    DdsImage() { type = DdsObjectType::Image; }
};

// =============================================================================
// DdsScene — the scene container
// =============================================================================

/**
 * @brief In-memory DDS scene graph.
 *
 * Maintains two parallel collections for O(1) direct access and O(n)
 * sequential traversal:
 *  - @c order – vector of object IDs in insertion order.
 *  - @c index – unordered_map from ID → shared_ptr<DdsObject>.
 *
 * Deletion is soft: objects are marked @c deleted so stable IDs are preserved.
 * Call compact() to physically remove soft-deleted entries.
 *
 * Thread safety: the caller (camera_api, ucs_api, dds_api, …) is responsible
 * for holding gMutex before calling any DdsScene method.
 */
class DdsScene
{
public:
    DdsScene() = default;
    ~DdsScene() = default;

    // Non-copyable (scene is always accessed through gState.dds)
    DdsScene(const DdsScene &) = delete;
    DdsScene &operator=(const DdsScene &) = delete;

    // -------------------------------------------------------------------------
    // Insert
    // -------------------------------------------------------------------------

    /** Assigns an auto-generated ID to @p obj and appends it to the scene. */
    std::shared_ptr<DdsObject> append(std::shared_ptr<DdsObject> obj);

    /**
     * Appends an object that already has its @c id field set (used during
     * deserialization to restore the original ID strings).  Updates @c nextId
     * if the id's numeric value exceeds the current counter.
     */
    std::shared_ptr<DdsObject> appendWithId(std::shared_ptr<DdsObject> obj);

    // -------------------------------------------------------------------------
    // Lookup
    // -------------------------------------------------------------------------

    /** Returns the object with the given @p id, or nullptr if not found / deleted. */
    std::shared_ptr<DdsObject> findById(const std::string &id) const;

    /**
     * Returns the first non-deleted object whose @c label equals @p label,
     * or nullptr if none found.
     */
    std::shared_ptr<DdsObject> findByLabel(const std::string &label) const;

    /** Returns the ID of the first non-deleted object with the given @p label, or "". */
    std::string findIdByLabel(const std::string &label) const;

    // -------------------------------------------------------------------------
    // Removal
    // -------------------------------------------------------------------------

    /** Soft-deletes the object with the given @p id.  Returns true on success. */
    bool remove(const std::string &id);

    /**
     * Clears all drawing-primitive objects (type not Camera, Ucs, WorldExtentsObj).
     * Camera and UCS objects remain in the scene.
     */
    void clearDrawingObjects();

    /** Clears ALL objects (cameras, UCS, drawing objects). */
    void clearAll();

    /** Physically removes soft-deleted entries from order and index. */
    void compact();

    // -------------------------------------------------------------------------
    // Introspection
    // -------------------------------------------------------------------------

    /** Returns the number of non-deleted objects in the scene. */
    int count() const;

    /** Returns the number of non-deleted drawing-primitive objects only. */
    int countDrawing() const;

    /**
     * Returns the insertion-order ID at position @p index (0-based).
     * Skips soft-deleted entries.  Returns "" if out of range.
     */
    std::string idAt(int index) const;

    // -------------------------------------------------------------------------
    // Traversal
    // -------------------------------------------------------------------------

    /** Calls @p fn(DdsObject&) for every non-deleted object in insertion order. */
    template <typename Fn>
    void forEach(Fn &&fn) const
    {
        for (const auto &id : order)
        {
            auto it = index.find(id);
            if (it != index.end() && !it->second->deleted)
                fn(*it->second);
        }
    }

    /** Calls @p fn(DdsObject&) for non-deleted drawing-primitive objects only. */
    template <typename Fn>
    void forEachDrawing(Fn &&fn) const
    {
        for (const auto &id : order)
        {
            auto it = index.find(id);
            if (it == index.end() || it->second->deleted)
                continue;
            const auto t = it->second->type;
            if (t == DdsObjectType::Camera || t == DdsObjectType::Ucs ||
                t == DdsObjectType::WorldExtentsObj)
                continue;
            fn(*it->second);
        }
    }

    // -------------------------------------------------------------------------
    // Data members (public for direct access by bgi_state.cpp only)
    // -------------------------------------------------------------------------
    std::vector<std::string>                                    order;
    std::unordered_map<std::string, std::shared_ptr<DdsObject>> index;
    uint64_t                                                    nextId{1};

private:
    std::string genId() { return std::to_string(nextId++); }
};

} // namespace bgi
