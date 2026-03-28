#include "bgi_ucs.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>

namespace bgi
{

    // -------------------------------------------------------------------------
    // Frame construction helpers
    // -------------------------------------------------------------------------

    CoordSystem ucsFromNormal(float nx, float ny, float nz,
                              float ox, float oy, float oz)
    {
        glm::vec3 zAxis = glm::normalize(glm::vec3(nx, ny, nz));

        // Choose a reference vector not parallel to zAxis for cross-product.
        // Prefer world-X; fall back to world-Y when zAxis ≈ world-X.
        const glm::vec3 ref = (std::abs(zAxis.x) < 0.9f)
                            ? glm::vec3(1.f, 0.f, 0.f)
                            : glm::vec3(0.f, 1.f, 0.f);

        glm::vec3 xAxis = glm::normalize(glm::cross(ref, zAxis));
        glm::vec3 yAxis = glm::normalize(glm::cross(zAxis, xAxis));

        CoordSystem cs;
        cs.originX = ox;  cs.originY = oy;  cs.originZ = oz;
        cs.xAxisX  = xAxis.x; cs.xAxisY = xAxis.y; cs.xAxisZ = xAxis.z;
        cs.yAxisX  = yAxis.x; cs.yAxisY = yAxis.y; cs.yAxisZ = yAxis.z;
        cs.zAxisX  = zAxis.x; cs.zAxisY = zAxis.y; cs.zAxisZ = zAxis.z;
        return cs;
    }

    void ucsOrthonormalise(CoordSystem &cs)
    {
        glm::vec3 x = glm::normalize(glm::vec3(cs.xAxisX, cs.xAxisY, cs.xAxisZ));
        glm::vec3 y = glm::normalize(glm::vec3(cs.yAxisX, cs.yAxisY, cs.yAxisZ));

        // Recompute Z = X × Y, then Y = Z × X for a clean right-handed frame.
        glm::vec3 z = glm::normalize(glm::cross(x, y));
        y = glm::normalize(glm::cross(z, x));

        cs.xAxisX = x.x; cs.xAxisY = x.y; cs.xAxisZ = x.z;
        cs.yAxisX = y.x; cs.yAxisY = y.y; cs.yAxisZ = y.z;
        cs.zAxisX = z.x; cs.zAxisY = z.y; cs.zAxisZ = z.z;
    }

    // -------------------------------------------------------------------------
    // Transform matrices
    // -------------------------------------------------------------------------

    glm::mat4 ucsLocalToWorldMatrix(const CoordSystem &cs)
    {
        // Column-major: columns are [X-axis | Y-axis | Z-axis | origin].
        return glm::mat4(
            cs.xAxisX, cs.xAxisY, cs.xAxisZ, 0.f,   // col 0 – X axis
            cs.yAxisX, cs.yAxisY, cs.yAxisZ, 0.f,   // col 1 – Y axis
            cs.zAxisX, cs.zAxisY, cs.zAxisZ, 0.f,   // col 2 – Z axis
            cs.originX, cs.originY, cs.originZ, 1.f  // col 3 – translation
        );
    }

    glm::mat4 ucsWorldToLocalMatrix(const CoordSystem &cs)
    {
        // For an orthonormal frame the inverse is the transpose (rotation part)
        // combined with a negated translated origin projected onto the axes.
        const glm::vec3 o(cs.originX, cs.originY, cs.originZ);
        const glm::vec3 x(cs.xAxisX,  cs.xAxisY,  cs.xAxisZ);
        const glm::vec3 y(cs.yAxisX,  cs.yAxisY,  cs.yAxisZ);
        const glm::vec3 z(cs.zAxisX,  cs.zAxisY,  cs.zAxisZ);

        return glm::mat4(
            x.x,            y.x,            z.x,            0.f,
            x.y,            y.y,            z.y,            0.f,
            x.z,            y.z,            z.z,            0.f,
           -glm::dot(x, o),-glm::dot(y, o),-glm::dot(z, o), 1.f
        );
    }

    // -------------------------------------------------------------------------
    // Point transforms
    // -------------------------------------------------------------------------

    void ucsWorldToLocal(const CoordSystem &cs,
                         float wx, float wy, float wz,
                         float &lx, float &ly, float &lz)
    {
        const glm::vec4 result =
            ucsWorldToLocalMatrix(cs) * glm::vec4(wx, wy, wz, 1.f);
        lx = result.x;
        ly = result.y;
        lz = result.z;
    }

    void ucsLocalToWorld(const CoordSystem &cs,
                         float lx, float ly, float lz,
                         float &wx, float &wy, float &wz)
    {
        const glm::vec4 result =
            ucsLocalToWorldMatrix(cs) * glm::vec4(lx, ly, lz, 1.f);
        wx = result.x;
        wy = result.y;
        wz = result.z;
    }

    // -------------------------------------------------------------------------
    // World extents helpers
    // -------------------------------------------------------------------------

    void worldExtentsExpand(WorldExtents &extents, float x, float y, float z)
    {
        if (!extents.hasData)
        {
            extents.minX = extents.maxX = x;
            extents.minY = extents.maxY = y;
            extents.minZ = extents.maxZ = z;
            extents.hasData = true;
        }
        else
        {
            extents.minX = std::min(extents.minX, x);
            extents.minY = std::min(extents.minY, y);
            extents.minZ = std::min(extents.minZ, z);
            extents.maxX = std::max(extents.maxX, x);
            extents.maxY = std::max(extents.maxY, y);
            extents.maxZ = std::max(extents.maxZ, z);
        }
    }

    glm::vec3 worldExtentsCentre(const WorldExtents &extents)
    {
        if (!extents.hasData)
            return glm::vec3(0.f);
        return glm::vec3(
            (extents.minX + extents.maxX) * 0.5f,
            (extents.minY + extents.maxY) * 0.5f,
            (extents.minZ + extents.maxZ) * 0.5f);
    }

    glm::vec3 worldExtentsHalfSize(const WorldExtents &extents)
    {
        if (!extents.hasData)
            return glm::vec3(0.f);
        return glm::vec3(
            (extents.maxX - extents.minX) * 0.5f,
            (extents.maxY - extents.minY) * 0.5f,
            (extents.maxZ - extents.minZ) * 0.5f);
    }

} // namespace bgi
