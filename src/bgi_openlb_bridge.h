#pragma once

#include "bgi_dds.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace bgi
{

/**
 * @brief Result of classifying a single world-space point against an OpenLB-tagged
 *        DDS scene.
 *
 * Populated by `openlbClassifyPoint()`.  The `matched` flag is the authoritative
 * indicator of whether any DDS object was found; all other fields are meaningful only
 * when `matched == true`.
 */
struct OpenLbMaterialHit
{
    /** `true` if a DDS object containing the query point was found; `false` otherwise. */
    bool        matched{false};

    /**
     * `true` when the matched object's `openlb.role` is anything other than
     * `"fluid"` (i.e., the point is inside a solid/wall/obstacle region).
     */
    bool        occupied{false};

    /**
     * Resolved material ID for the matched object.  Derived in priority order from
     * the `openlb.material` attribute (if present), then from `openlb.role`
     * (`"fluid"` → `defaultFluidMaterial`, otherwise → `defaultSolidMaterial`).
     * Zero when `matched == false`.
     */
    int         material{0};

    /**
     * Sort key used to resolve conflicts when multiple DDS objects contain the
     * same point.  Populated from the `openlb.priority` attribute (integer).
     * Higher values take precedence.  Defaults to 0 when the attribute is absent.
     */
    int         priority{0};

    /** ID string of the matched DDS object.  Empty when `matched == false`. */
    std::string objectId;

    /**
     * Lower-cased value of the `openlb.role` attribute of the matched object.
     * Typical values: `"fluid"`, `"solid"`, `"wall"`, `"obstacle"`.
     * Empty when the attribute is absent or when `matched == false`.
     */
    std::string role;

    /**
     * Value of the `openlb.boundary` attribute of the matched object.
     * Typical values: `"wall"`, `"obstacle"`, `"inlet"`, `"outlet"`.
     * Empty when the attribute is absent or when `matched == false`.
     */
    std::string boundary;
};

/**
 * @brief Classify a single world-space point against all OpenLB-tagged render roots in
 *        a DDS scene and populate a hit record.
 *
 * Traverses the scene's render-root list and, for each root, recursively walks the
 * DDS object graph (handling Transforms, set-union, set-intersection, set-difference,
 * and leaf primitives).  When multiple objects contain the point, the one with the
 * highest `openlb.priority` attribute wins; ties are resolved by the last candidate
 * found (last insertion order).
 *
 * An object participates only if:
 * - It is visible, not deleted, and not disabled via `openlb.enabled = "0"` / `"false"`
 *   / `"off"`.
 * - It carries at least one attribute whose key begins with `"openlb."`.
 *
 * @param[in]  scene                The DDS scene to query (read-only).
 * @param[in]  worldPoint           Query point in world (physical) coordinates.
 * @param[out] hit                  Hit record populated on return.  Always
 *                                  re-initialised to a default `OpenLbMaterialHit`
 *                                  before traversal starts.
 * @param[in]  defaultFluidMaterial Material ID used when an object's role resolves to
 *                                  `"fluid"`.  Defaults to 1.
 * @param[in]  defaultSolidMaterial Material ID used when an object has no explicit
 *                                  material and its role is not `"fluid"`.  Defaults
 *                                  to 2.
 *
 * @return `true` if at least one DDS object was matched; `false` otherwise.
 *
 * @note This function is **not** thread-safe.  The caller must hold `bgi::gMutex` (or
 *       ensure no other thread modifies the scene) for the duration of the call.
 * @note Point-in-solid tests use a small epsilon (~1e-4 world units) so points very
 *       close to object boundaries may be classified as inside.
 */
bool openlbClassifyPoint(const DdsScene &scene,
                         const glm::vec3 &worldPoint,
                         OpenLbMaterialHit &hit,
                         int defaultFluidMaterial = 1,
                         int defaultSolidMaterial = 2);

/**
 * @brief Sample a 2-D axis-aligned grid of world-space points and write per-cell
 *        material IDs into a `std::vector<int>`.
 *
 * Resizes @p materials to `cols * rows` and fills it with `defaultFluidMaterial`.
 * Then iterates every cell `(col, row)` in row-major order and samples the DDS scene
 * at the cell's centre:
 * @code
 * x = minX + (col + 0.5f) * stepX
 * y = minY + (row + 0.5f) * stepY
 * @endcode
 * Output index: `materials[row * cols + col]`.  Unmatched cells retain
 * `defaultFluidMaterial`.
 *
 * @param[in]  scene                DDS scene to query (read-only).
 * @param[in]  minX                 X world coordinate of the grid's lower-left corner.
 * @param[in]  minY                 Y world coordinate of the grid's lower-left corner.
 * @param[in]  z                    Constant Z world coordinate for all sample points.
 * @param[in]  cols                 Number of grid columns (X direction).
 * @param[in]  rows                 Number of grid rows (Y direction).
 * @param[in]  stepX                Cell width in world units.
 * @param[in]  stepY                Cell height in world units.
 * @param[out] materials            Output vector.  Resized to `max(0, cols*rows)` and
 *                                  populated with material IDs on return.
 * @param[in]  defaultFluidMaterial Fill value for unmatched cells and fluid-role
 *                                  objects.  Defaults to 1.
 * @param[in]  defaultSolidMaterial Solid-role fallback passed to
 *                                  `openlbClassifyPoint`.  Defaults to 2.
 *
 * @note If `cols <= 0` or `rows <= 0` the function returns immediately after resizing
 *       @p materials to 0.
 * @note This function is **not** thread-safe.  The caller must hold `bgi::gMutex` (or
 *       ensure no other thread modifies the scene) for the duration of the call.
 */
void openlbSampleMaterials2D(const DdsScene &scene,
                             float minX, float minY, float z,
                             int cols, int rows,
                             float stepX, float stepY,
                             std::vector<int> &materials,
                             int defaultFluidMaterial = 1,
                             int defaultSolidMaterial = 2);

} // namespace bgi
