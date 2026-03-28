#pragma once

#include "bgi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * @file bgi_ucs.h
 * @brief Internal GLM-based math for User Coordinate System (UCS) transforms.
 *
 * A UCS is stored as a @c CoordSystem value: an origin point plus three
 * orthonormal axis vectors, all expressed in world space.
 *
 * **Coordinate convention**: Z-up, right-handed world space throughout.
 *
 * None of these functions are part of the public C API.  Consumers use the
 * @c wxbgi_ucs_* functions declared in wx_bgi_3d.h.
 */

namespace bgi
{

    // -------------------------------------------------------------------------
    // Frame construction helpers
    // -------------------------------------------------------------------------

    /**
     * Constructs a right-handed orthonormal frame from a surface normal.
     *
     * The normal becomes the UCS Z axis.  An arbitrary perpendicular is chosen
     * for X and Y using the Gram-Schmidt process, with a fallback that avoids
     * degeneracy when the normal is parallel to the world Z axis.
     *
     * @param nx,ny,nz  Surface normal (need not be unit length).
     * @param ox,oy,oz  Origin of the new UCS in world space.
     * @return          Fully populated @c CoordSystem with unit-length axes.
     */
    CoordSystem ucsFromNormal(float nx, float ny, float nz,
                              float ox, float oy, float oz);

    /**
     * Orthonormalises the three axis vectors of a CoordSystem in-place using
     * the Gram-Schmidt process.  Z is recomputed as X × Y, then Y is
     * recomputed as Z × X to guarantee a right-handed system.
     */
    void ucsOrthonormalise(CoordSystem &cs);

    // -------------------------------------------------------------------------
    // Transform matrices
    // -------------------------------------------------------------------------

    /**
     * Returns the 4×4 matrix that transforms a point from UCS local space to
     * world space.  This is the standard "model" matrix for objects defined in
     * UCS coordinates.
     */
    glm::mat4 ucsLocalToWorldMatrix(const CoordSystem &cs);

    /**
     * Returns the 4×4 matrix that transforms a point from world space to UCS
     * local space.  This is the inverse of @ref ucsLocalToWorldMatrix.
     */
    glm::mat4 ucsWorldToLocalMatrix(const CoordSystem &cs);

    // -------------------------------------------------------------------------
    // Point transforms
    // -------------------------------------------------------------------------

    /**
     * Transforms a world-space point into UCS local coordinates.
     *
     * @param cs      UCS to transform into.
     * @param wx,wy,wz  World-space input.
     * @param lx,ly,lz  Output UCS local coordinates.
     */
    void ucsWorldToLocal(const CoordSystem &cs,
                         float wx, float wy, float wz,
                         float &lx, float &ly, float &lz);

    /**
     * Transforms a UCS local point into world-space coordinates.
     *
     * @param cs        UCS to transform from.
     * @param lx,ly,lz  UCS local input.
     * @param wx,wy,wz  Output world-space coordinates.
     */
    void ucsLocalToWorld(const CoordSystem &cs,
                         float lx, float ly, float lz,
                         float &wx, float &wy, float &wz);

    // -------------------------------------------------------------------------
    // World extents helpers
    // -------------------------------------------------------------------------

    /**
     * Expands @p extents to include the point (@p x, @p y, @p z).
     * Sets @c extents.hasData = @c true on first call.
     */
    void worldExtentsExpand(WorldExtents &extents, float x, float y, float z);

    /**
     * Returns the centre of the AABB as a vec3.
     * Returns the origin if @c extents.hasData is @c false.
     */
    glm::vec3 worldExtentsCentre(const WorldExtents &extents);

    /**
     * Returns the half-extents (half-size of each axis) as a vec3.
     * Returns (0,0,0) if @c extents.hasData is @c false.
     */
    glm::vec3 worldExtentsHalfSize(const WorldExtents &extents);

} // namespace bgi
