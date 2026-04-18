#include "wx_bgi_openlb.h"

#include <mutex>
#include <vector>

#include "bgi_openlb_bridge.h"
#include "bgi_state.h"

BGI_API int BGI_CALL wxbgi_openlb_classify_point_material(float x, float y, float z,
                                                          int defaultFluidMaterial,
                                                          int defaultSolidMaterial,
                                                          int *outMatched)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (outMatched != nullptr)
        *outMatched = 0;
    if (bgi::gState.dds == nullptr)
        return defaultFluidMaterial;

    bgi::OpenLbMaterialHit hit;
    const bool matched = bgi::openlbClassifyPoint(*bgi::gState.dds,
                                                  {x, y, z},
                                                  hit,
                                                  defaultFluidMaterial,
                                                  defaultSolidMaterial);
    if (outMatched != nullptr)
        *outMatched = matched ? 1 : 0;
    return matched ? hit.material : defaultFluidMaterial;
}

BGI_API int BGI_CALL wxbgi_openlb_sample_materials_2d(float minX, float minY, float z,
                                                      int cols, int rows,
                                                      float stepX, float stepY,
                                                      int defaultFluidMaterial,
                                                      int defaultSolidMaterial,
                                                      int *outMaterials,
                                                      int materialCount)
{
    if (cols <= 0 || rows <= 0 || outMaterials == nullptr)
        return -1;

    const int expected = cols * rows;
    if (materialCount < expected)
        return -1;

    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (bgi::gState.dds == nullptr)
        return -1;

    std::vector<int> sampled;
    bgi::openlbSampleMaterials2D(*bgi::gState.dds,
                                 minX, minY, z,
                                 cols, rows,
                                 stepX, stepY,
                                 sampled,
                                 defaultFluidMaterial,
                                 defaultSolidMaterial);
    for (int i = 0; i < expected; ++i)
        outMaterials[i] = sampled[static_cast<std::size_t>(i)];
    return expected;
}
