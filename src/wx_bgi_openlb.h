#pragma once

#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#ifdef __cplusplus
#include <utility>

namespace olb
{
template <typename T, unsigned D>
class SuperGeometry;
}
#endif

/**
 * @file wx_bgi_openlb.h
 * @brief Convenience wrappers and C++ templates for driving wx_bgi from an OpenLB-style
 *        live loop and for materializing DDS scene geometry into an `olb::SuperGeometry`.
 *
 * This header provides three layers of functionality:
 *
 * **1. Session helpers** (`wxbgi_openlb_begin_session`, `wxbgi_openlb_pump`,
 *    `wxbgi_openlb_present`) — thin inline wrappers that open a wx_bgi window and
 *    run the event loop inside an OpenLB solver's time-stepping loop, matching the
 *    conventional OpenLB `while (pump())` idiom.
 *
 * **2. C bridge functions** (`wxbgi_openlb_classify_point_material`,
 *    `wxbgi_openlb_sample_materials_2d`) — exported C functions that query the active
 *    DDS retained scene and return integer OpenLB material IDs for arbitrary physical
 *    coordinates.  All C functions acquire the internal `bgi::gMutex` and are safe to
 *    call from any thread.
 *
 * **3. C++ materialize templates** (`wxbgi_openlb_materialize_super_geometry_2d`,
 *    `wxbgi_openlb_materialize_super_geometry_3d`) — header-only function templates
 *    that iterate every core lattice node of an `olb::SuperGeometry`, project its
 *    physical coordinates into the DDS scene, and write the resolved material IDs back
 *    into the geometry.  An optional `Mapper` callback gives callers full control over
 *    the final per-node material assignment.
 *
 * @note The C++ templates are **not** independently thread-safe.  Call them from a
 *       single thread while no other thread is modifying the DDS scene.
 *
 * @note Coordinate units must be consistent: all physical coordinates passed to the
 *       bridge functions must be in the same world-space units used by the DDS scene
 *       (typically SI metres when working with OpenLB).
 */

/**
 * @brief Create a wx_bgi window and prepare it for use inside an OpenLB time loop.
 *
 * This is a convenience wrapper for the three-call sequence that every OpenLB-coupled
 * demo requires:
 * 1. `wxbgi_wx_app_create()` — create the wxWidgets application object.
 * 2. `wxbgi_wx_frame_create(width, height, title)` — open the window.
 * 3. `wxbgi_wx_set_frame_rate(0)` — disable the internal frame-rate cap so the solver
 *    loop can present frames at its own pace.
 *
 * Call this function **once** at the start of `main()`, before creating any DDS scene
 * objects, cameras, or lattice structures.
 *
 * @param[in] width  Width of the window in pixels (must be > 0).
 * @param[in] height Height of the window in pixels (must be > 0).
 * @param[in] title  Null-terminated UTF-8 window title string.  May be `NULL`, in
 *                   which case the platform default title is used.
 *
 * @note After this call returns, `graphresult()` returns 0 on success.
 * @note This function is **not** thread-safe; call it from the main thread only.
 */
static inline void wxbgi_openlb_begin_session(int width, int height, const char* title)
{
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(width, height, title);
    wxbgi_wx_set_frame_rate(0);
}

/**
 * @brief Initialise wx_bgi for use inside a caller-owned wxWidgets frame/canvas.
 *
 * Use this helper when your application already owns the wxWidgets event loop
 * and embeds `wxbgi::WxBgiCanvas` inside a custom `wxFrame`. It prepares the
 * BGI page buffers for the canvas size without creating a standalone frame.
 *
 * @param[in] width  Initial embedded canvas width in pixels (must be > 0).
 * @param[in] height Initial embedded canvas height in pixels (must be > 0).
 */
static inline void wxbgi_openlb_begin_canvas_session(int width, int height)
{
    wxbgi_wx_init_for_canvas(width, height);
}

/**
 * @brief Process pending GUI events and report whether the simulation loop should
 *        continue.
 *
 * Call this function at the top of every solver time step to keep the wx_bgi window
 * responsive.  It pumps pending window events (resize, keyboard, close) without
 * flushing any drawing to the screen.
 *
 * @return 1 if the window is still open and the loop should continue;
 *         0 if the user has requested that the window be closed (i.e.,
 *         `wxbgi_should_close()` returns non-zero).
 *
 * @note This function is safe to call from the main thread only.
 * @see wxbgi_openlb_present() for a combined "flush + pump" variant.
 */
static inline int wxbgi_openlb_pump(void)
{
    if (wxbgi_should_close() != 0)
        return 0;
    wxbgi_poll_events();
    return wxbgi_should_close() == 0 ? 1 : 0;
}

/**
 * @brief Flush the current frame to the screen and pump GUI events.
 *
 * Combines `wxbgi_wx_refresh()` (which copies the back-buffer to the display) with a
 * call to `wxbgi_openlb_pump()`.  Use this at the end of each visualization step:
 *
 * @code
 * while (wxbgi_openlb_pump()) {
 *     // ... solver step and draw calls ...
 *     if (!wxbgi_openlb_present()) break;
 * }
 * @endcode
 *
 * @return 1 if the window is still open; 0 if the user closed the window (same
 *         semantics as `wxbgi_openlb_pump()`).
 *
 * @note This function is safe to call from the main thread only.
 * @see wxbgi_openlb_pump() for pumping without flushing.
 */
static inline int wxbgi_openlb_present(void)
{
    wxbgi_wx_refresh();
    return wxbgi_openlb_pump();
}

/**
 * @brief Classify a single physical-space point against the active DDS scene and
 *        return the associated OpenLB material ID.
 *
 * The function traverses the active DDS retained scene (the scene set via
 * `wxbgi_dds_scene_set_active()`, or the default scene) and tests whether the world
 * point `(x, y, z)` falls inside any DDS object that carries `openlb.*` attributes.
 * The first matching object (highest `openlb.priority`, then insertion order) determines
 * the returned material.
 *
 * Objects participate in classification only when:
 * - They are visible and not deleted.
 * - They have at least one `openlb.*` external attribute.
 * - Their `openlb.enabled` attribute is absent, or is not `"0"`, `"false"`, or `"off"`.
 *
 * Material resolution order for a matching object:
 * 1. `openlb.material` attribute (integer) — used directly if present.
 * 2. `openlb.role` attribute — `"fluid"` → `defaultFluidMaterial`; anything else →
 *    `defaultSolidMaterial`.
 *
 * @param[in]  x                    X coordinate of the query point in world (physical)
 *                                  units.
 * @param[in]  y                    Y coordinate of the query point in world (physical)
 *                                  units.
 * @param[in]  z                    Z coordinate of the query point in world (physical)
 *                                  units.
 * @param[in]  defaultFluidMaterial Material ID to return when no DDS object is matched
 *                                  and as the default fluid-role fallback.  Typically 1
 *                                  (OpenLB convention).
 * @param[in]  defaultSolidMaterial Material ID used as the default solid-role fallback.
 *                                  Typically 2.
 * @param[out] outMatched           If not `NULL`, set to `1` when a DDS object was
 *                                  matched, or `0` when the point is unmatched.  The
 *                                  caller can use this flag to distinguish between a
 *                                  genuine fluid material and the default fallback.
 *
 * @return The integer material ID of the highest-priority matching DDS object, or
 *         `defaultFluidMaterial` when no object contains the point.
 *
 * @note This function acquires `bgi::gMutex` and is safe to call from any thread.
 * @note If no DDS scene is active (`gState.dds == nullptr`), the function immediately
 *       returns `defaultFluidMaterial` with `*outMatched = 0`.
 * @note Point-in-solid tests use a small numerical epsilon (~1e-4 world units) so
 *       boundary points may be counted as inside.
 */
BGI_API int BGI_CALL wxbgi_openlb_classify_point_material(float x, float y, float z,
                                                          int defaultFluidMaterial,
                                                          int defaultSolidMaterial,
                                                          int *outMatched);

/**
 * @brief Sample a 2-D axis-aligned grid of physical points against the active DDS
 *        scene and write per-cell material IDs into a caller-supplied array.
 *
 * The function evaluates `cols × rows` sample points arranged in a regular grid.
 * Each cell `(col, row)` (0-based) is sampled at its **centre**:
 * @code
 * x = minX + (col + 0.5f) * stepX
 * y = minY + (row + 0.5f) * stepY
 * z = z  (constant for all cells)
 * @endcode
 *
 * The output array is laid out **row-major**: `outMaterials[row * cols + col]`.
 * Unmatched cells are filled with `defaultFluidMaterial`.
 *
 * @param[in]  minX                 X coordinate of the lower-left corner of the grid
 *                                  in world (physical) units.
 * @param[in]  minY                 Y coordinate of the lower-left corner of the grid
 *                                  in world (physical) units.
 * @param[in]  z                    Constant Z coordinate at which all cells are sampled.
 * @param[in]  cols                 Number of grid columns (X direction, must be > 0).
 * @param[in]  rows                 Number of grid rows (Y direction, must be > 0).
 * @param[in]  stepX                Cell width in world units (`> 0`).
 * @param[in]  stepY                Cell height in world units (`> 0`).
 * @param[in]  defaultFluidMaterial Material ID written for unmatched cells.
 * @param[in]  defaultSolidMaterial Fallback solid-role material (passed to
 *                                  `wxbgi_openlb_classify_point_material`).
 * @param[out] outMaterials         Caller-allocated array of at least `materialCount`
 *                                  `int` elements.  On success, the first `cols * rows`
 *                                  entries are overwritten in row-major order.  Must
 *                                  not be `NULL`.
 * @param[in]  materialCount        Size of the `outMaterials` array.  Must be ≥
 *                                  `cols * rows`; the function returns `-1` if the
 *                                  provided buffer is too small.
 *
 * @return The number of array elements written (`cols * rows`) on success, or `-1`
 *         if any of the following conditions hold:
 *         - `cols ≤ 0` or `rows ≤ 0`
 *         - `outMaterials == NULL`
 *         - `materialCount < cols * rows`
 *         - No active DDS scene is present.
 *
 * @note This function acquires `bgi::gMutex` and is safe to call from any thread.
 * @note Ownership of `outMaterials` remains with the caller; the function never frees
 *       or reallocates it.
 * @see wxbgi_openlb_classify_point_material() for single-point classification.
 */
BGI_API int BGI_CALL wxbgi_openlb_sample_materials_2d(float minX, float minY, float z,
                                                      int cols, int rows,
                                                      float stepX, float stepY,
                                                      int defaultFluidMaterial,
                                                      int defaultSolidMaterial,
                                                      int *outMaterials,
                                                      int materialCount);

#ifdef __cplusplus

/**
 * @brief Statistics returned by the `wxbgi_openlb_materialize_super_geometry_*`
 *        functions.
 *
 * All counters are non-negative.  They refer to the lattice nodes processed during a
 * single call to a materialize function and can be used to verify that the DDS scene
 * overlaps the OpenLB domain as expected.
 *
 * Example use:
 * @code
 * WxbgiOpenLbMaterializeStats stats =
 *     wxbgi_openlb_materialize_super_geometry_2d(superGeometry, T(0));
 * printf("visited=%d  matched=%d  updated=%d\n",
 *        stats.visited, stats.matched, stats.updated);
 * @endcode
 */
struct WxbgiOpenLbMaterializeStats
{
    /** Total number of core lattice nodes visited. */
    int visited{0};

    /**
     * Number of visited nodes for which the DDS scene contained a matching object
     * (i.e., `wxbgi_openlb_classify_point_material` returned `outMatched = 1`).
     */
    int matched{0};

    /**
     * Number of nodes whose material was actually changed.  A node is counted here
     * only when the `Mapper` returned a material different from the node's current
     * material.
     */
    int updated{0};
};

/**
 * @brief Materialize DDS scene objects into a 2-D `olb::SuperGeometry` using a custom
 *        material-mapping callback.
 *
 * Iterates every core spatial location in @p superGeometry, converts each lattice
 * coordinate to a physical (world) coordinate via `getPhysR`, and calls
 * `wxbgi_openlb_classify_point_material` with the Z-slice coordinate @p z.  The
 * resolved bridge material and the match flag are forwarded to @p mapMaterial, whose
 * return value is written back into the geometry.
 *
 * After all nodes are processed, the function calls `superGeometry.communicate()` and
 * updates geometry statistics so that subsequent `superGeometry.print()` and
 * `rename()`/`clean()` calls see consistent data.
 *
 * **Mapper callback contract**
 *
 * The callback must be callable with the following signature (or a compatible lambda):
 * @code
 * int mapMaterial(const auto &physR, int currentMaterial,
 *                 int bridgeMaterial, bool matched);
 * @endcode
 * where:
 * - @p physR — physical coordinate vector returned by `getPhysR` (indexing: `physR[0]`
 *   = X, `physR[1]` = Y).
 * - @p currentMaterial — the material currently stored at this lattice node before any
 *   change.
 * - @p bridgeMaterial — the material ID returned by
 *   `wxbgi_openlb_classify_point_material`.  When `matched == false` this equals
 *   `defaultFluidMaterial`.
 * - @p matched — `true` if the DDS scene contained an object at this location;
 *   `false` if the point was unmatched and `overwriteUnmatched` is `true`.
 *
 * The callback must return the desired new material ID for the node.  If it returns the
 * same value as `currentMaterial`, the node is left unchanged (and is **not** counted
 * in `WxbgiOpenLbMaterializeStats::updated`).
 *
 * @tparam T      OpenLB floating-point precision type (typically `double`).
 * @tparam Mapper Callable type satisfying the signature above.
 *
 * @param[in,out] superGeometry      The 2-D OpenLB super geometry to modify.
 * @param[in]     z                  Z coordinate (in physical units) used as the
 *                                   constant slice depth when projecting 2-D lattice
 *                                   nodes into the 3-D DDS scene.
 * @param[in]     mapMaterial        Callback invoked for every node that should be
 *                                   updated (see contract above).
 * @param[in]     defaultFluidMaterial Material ID passed to
 *                                   `wxbgi_openlb_classify_point_material` as the
 *                                   fluid default; also the fallback returned for
 *                                   unmatched nodes.  Default: 1.
 * @param[in]     defaultSolidMaterial Material ID passed to
 *                                   `wxbgi_openlb_classify_point_material` as the
 *                                   solid default.  Default: 2.
 * @param[in]     overwriteUnmatched If `false` (default), nodes with no DDS match are
 *                                   skipped entirely and the Mapper is not called for
 *                                   them.  If `true`, the Mapper is called even for
 *                                   unmatched nodes so it can apply a custom fallback.
 *
 * @return A `WxbgiOpenLbMaterializeStats` struct with counters for visited, matched,
 *         and updated nodes.
 *
 * @note Each per-node call to `wxbgi_openlb_classify_point_material` acquires and
 *       releases `bgi::gMutex`.  Do not modify the DDS scene from another thread
 *       while this function is running.
 * @note Typical usage places this call after `superGeometry.rename()` / `clean()` and
 *       before `prepareLattice()`.
 * @see wxbgi_openlb_materialize_super_geometry_2d(olb::SuperGeometry<T,2>&, T, int, int, bool)
 *      for the simpler overload that does not require a custom Mapper.
 */
template <typename T, typename Mapper>
inline WxbgiOpenLbMaterializeStats wxbgi_openlb_materialize_super_geometry_2d(
    olb::SuperGeometry<T, 2> &superGeometry,
    T z,
    Mapper &&mapMaterial,
    int defaultFluidMaterial = 1,
    int defaultSolidMaterial = 2,
    bool overwriteUnmatched = false)
{
    WxbgiOpenLbMaterializeStats stats;

    superGeometry.forCoreSpatialLocations([&](auto latticeR) {
        ++stats.visited;

        const auto physR = superGeometry.getPhysR(latticeR);

        int matched = 0;
        const int bridgeMaterial = wxbgi_openlb_classify_point_material(
            static_cast<float>(physR[0]),
            static_cast<float>(physR[1]),
            static_cast<float>(z),
            defaultFluidMaterial,
            defaultSolidMaterial,
            &matched);

        if (matched != 0)
            ++stats.matched;
        else if (!overwriteUnmatched)
            return;

        auto &blockGeometry = superGeometry.getBlockGeometry(latticeR[0]);
        const int localR[2] = {latticeR[1], latticeR[2]};
        const int currentMaterial = blockGeometry.get(localR);
        const int nextMaterial = mapMaterial(physR, currentMaterial, bridgeMaterial, matched != 0);
        if (nextMaterial == currentMaterial)
            return;

        blockGeometry.set(localR, nextMaterial);
        ++stats.updated;
    });

    superGeometry.communicate();
    superGeometry.getStatisticsStatus() = true;
    superGeometry.updateStatistics(false);
    return stats;
}

/**
 * @brief Materialize DDS scene objects into a 2-D `olb::SuperGeometry` using the
 *        default material-mapping strategy.
 *
 * This is a simplified overload of the Mapper variant.  For every matched lattice
 * node it writes `bridgeMaterial` directly (i.e., the material resolved from the DDS
 * scene).  Unmatched nodes are handled according to `overwriteUnmatched`:
 * - `false` (default) — unmatched nodes are left unchanged.
 * - `true` — unmatched nodes are set to `defaultFluidMaterial`.
 *
 * @tparam T OpenLB floating-point precision type (typically `double`).
 *
 * @param[in,out] superGeometry      The 2-D OpenLB super geometry to modify.
 * @param[in]     z                  Z coordinate (physical units) for the 2-D slice.
 * @param[in]     defaultFluidMaterial Fluid-role fallback and unmatched fill value.
 *                                   Default: 1.
 * @param[in]     defaultSolidMaterial Solid-role fallback passed to the bridge.
 *                                   Default: 2.
 * @param[in]     overwriteUnmatched If `true`, unmatched nodes are set to
 *                                   `defaultFluidMaterial`; if `false` they are
 *                                   skipped.  Default: `false`.
 *
 * @return A `WxbgiOpenLbMaterializeStats` struct (same semantics as the Mapper
 *         overload).
 *
 * @see wxbgi_openlb_materialize_super_geometry_2d(olb::SuperGeometry<T,2>&, T, Mapper&&, int, int, bool)
 *      for the full-control Mapper overload.
 */
template <typename T>
inline WxbgiOpenLbMaterializeStats wxbgi_openlb_materialize_super_geometry_2d(
    olb::SuperGeometry<T, 2> &superGeometry,
    T z,
    int defaultFluidMaterial = 1,
    int defaultSolidMaterial = 2,
    bool overwriteUnmatched = false)
{
    return wxbgi_openlb_materialize_super_geometry_2d(
        superGeometry,
        z,
        [defaultFluidMaterial](const auto &, int, int bridgeMaterial, bool matched) {
            return matched ? bridgeMaterial : defaultFluidMaterial;
        },
        defaultFluidMaterial,
        defaultSolidMaterial,
        overwriteUnmatched);
}

/**
 * @brief Materialize DDS scene objects into a 3-D `olb::SuperGeometry` using a custom
 *        material-mapping callback.
 *
 * Identical in purpose to the 2-D variant, but operates on a 3-D geometry.  Each
 * lattice node's physical coordinates `(physR[0], physR[1], physR[2])` are all three
 * passed to `wxbgi_openlb_classify_point_material` — there is no Z-slice parameter.
 *
 * **Mapper callback contract**
 *
 * The callback must be callable with the following signature (or a compatible lambda):
 * @code
 * int mapMaterial(const auto &physR, int currentMaterial,
 *                 int bridgeMaterial, bool matched);
 * @endcode
 * where:
 * - @p physR — physical coordinate vector (`physR[0]` = X, `physR[1]` = Y,
 *   `physR[2]` = Z).
 * - @p currentMaterial — material currently stored at the node.
 * - @p bridgeMaterial — material from the DDS bridge (equals `defaultFluidMaterial`
 *   when `matched == false`).
 * - @p matched — `true` if a DDS object was found at this node.
 *
 * The return value is the desired new material; return `currentMaterial` to leave the
 * node unchanged.
 *
 * @tparam T      OpenLB floating-point precision type (typically `double`).
 * @tparam Mapper Callable type satisfying the signature above.
 *
 * @param[in,out] superGeometry      The 3-D OpenLB super geometry to modify.
 * @param[in]     mapMaterial        Callback invoked for each node to be updated.
 * @param[in]     defaultFluidMaterial Fluid-role fallback.  Default: 1.
 * @param[in]     defaultSolidMaterial Solid-role fallback.  Default: 2.
 * @param[in]     overwriteUnmatched If `false` (default), unmatched nodes are skipped.
 *                                   If `true`, the Mapper is also called for unmatched
 *                                   nodes.
 *
 * @return A `WxbgiOpenLbMaterializeStats` struct with visited/matched/updated counts.
 *
 * @note Do not modify the DDS scene from another thread while this function is running.
 * @see wxbgi_openlb_materialize_super_geometry_3d(olb::SuperGeometry<T,3>&, int, int, bool)
 *      for the simpler overload without a custom Mapper.
 */
template <typename T, typename Mapper>
inline WxbgiOpenLbMaterializeStats wxbgi_openlb_materialize_super_geometry_3d(
    olb::SuperGeometry<T, 3> &superGeometry,
    Mapper &&mapMaterial,
    int defaultFluidMaterial = 1,
    int defaultSolidMaterial = 2,
    bool overwriteUnmatched = false)
{
    WxbgiOpenLbMaterializeStats stats;

    superGeometry.forCoreSpatialLocations([&](auto latticeR) {
        ++stats.visited;

        const auto physR = superGeometry.getPhysR(latticeR);

        int matched = 0;
        const int bridgeMaterial = wxbgi_openlb_classify_point_material(
            static_cast<float>(physR[0]),
            static_cast<float>(physR[1]),
            static_cast<float>(physR[2]),
            defaultFluidMaterial,
            defaultSolidMaterial,
            &matched);

        if (matched != 0)
            ++stats.matched;
        else if (!overwriteUnmatched)
            return;

        auto &blockGeometry = superGeometry.getBlockGeometry(latticeR[0]);
        const int localR[3] = {latticeR[1], latticeR[2], latticeR[3]};
        const int currentMaterial = blockGeometry.get(localR);
        const int nextMaterial = mapMaterial(physR, currentMaterial, bridgeMaterial, matched != 0);
        if (nextMaterial == currentMaterial)
            return;

        blockGeometry.set(localR, nextMaterial);
        ++stats.updated;
    });

    superGeometry.communicate();
    superGeometry.getStatisticsStatus() = true;
    superGeometry.updateStatistics(false);
    return stats;
}

/**
 * @brief Materialize DDS scene objects into a 3-D `olb::SuperGeometry` using the
 *        default material-mapping strategy.
 *
 * Simplified overload of the Mapper variant for 3-D geometries.  Matched nodes receive
 * the material resolved from the DDS bridge; unmatched nodes are either left unchanged
 * (`overwriteUnmatched = false`, default) or set to `defaultFluidMaterial`
 * (`overwriteUnmatched = true`).
 *
 * @tparam T OpenLB floating-point precision type (typically `double`).
 *
 * @param[in,out] superGeometry      The 3-D OpenLB super geometry to modify.
 * @param[in]     defaultFluidMaterial Fluid-role fallback and unmatched fill value.
 *                                   Default: 1.
 * @param[in]     defaultSolidMaterial Solid-role fallback passed to the bridge.
 *                                   Default: 2.
 * @param[in]     overwriteUnmatched If `true`, unmatched nodes are set to
 *                                   `defaultFluidMaterial`.  Default: `false`.
 *
 * @return A `WxbgiOpenLbMaterializeStats` struct.
 *
 * @see wxbgi_openlb_materialize_super_geometry_3d(olb::SuperGeometry<T,3>&, Mapper&&, int, int, bool)
 *      for the full-control Mapper overload.
 */
template <typename T>
inline WxbgiOpenLbMaterializeStats wxbgi_openlb_materialize_super_geometry_3d(
    olb::SuperGeometry<T, 3> &superGeometry,
    int defaultFluidMaterial = 1,
    int defaultSolidMaterial = 2,
    bool overwriteUnmatched = false)
{
    return wxbgi_openlb_materialize_super_geometry_3d(
        superGeometry,
        [defaultFluidMaterial](const auto &, int, int bridgeMaterial, bool matched) {
            return matched ? bridgeMaterial : defaultFluidMaterial;
        },
        defaultFluidMaterial,
        defaultSolidMaterial,
        overwriteUnmatched);
}

#endif
