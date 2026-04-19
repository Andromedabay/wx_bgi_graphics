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
 * @brief Convenience wrappers for driving wx_bgi from an OpenLB-style live loop.
 */

static inline void wxbgi_openlb_begin_session(int width, int height, const char* title)
{
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(width, height, title);
    wxbgi_wx_set_frame_rate(0);
}

static inline int wxbgi_openlb_pump(void)
{
    if (wxbgi_should_close() != 0)
        return 0;
    wxbgi_poll_events();
    return wxbgi_should_close() == 0 ? 1 : 0;
}

static inline int wxbgi_openlb_present(void)
{
    wxbgi_wx_refresh();
    return wxbgi_openlb_pump();
}

BGI_API int BGI_CALL wxbgi_openlb_classify_point_material(float x, float y, float z,
                                                          int defaultFluidMaterial,
                                                          int defaultSolidMaterial,
                                                          int *outMatched);

BGI_API int BGI_CALL wxbgi_openlb_sample_materials_2d(float minX, float minY, float z,
                                                      int cols, int rows,
                                                      float stepX, float stepY,
                                                      int defaultFluidMaterial,
                                                      int defaultSolidMaterial,
                                                      int *outMaterials,
                                                      int materialCount);

#ifdef __cplusplus

struct WxbgiOpenLbMaterializeStats
{
    int visited{0};
    int matched{0};
    int updated{0};
};

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
