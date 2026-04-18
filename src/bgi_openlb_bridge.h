#pragma once

#include "bgi_dds.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace bgi
{

struct OpenLbMaterialHit
{
    bool        matched{false};
    bool        occupied{false};
    int         material{0};
    int         priority{0};
    std::string objectId;
    std::string role;
    std::string boundary;
};

bool openlbClassifyPoint(const DdsScene &scene,
                         const glm::vec3 &worldPoint,
                         OpenLbMaterialHit &hit,
                         int defaultFluidMaterial = 1,
                         int defaultSolidMaterial = 2);

void openlbSampleMaterials2D(const DdsScene &scene,
                             float minX, float minY, float z,
                             int cols, int rows,
                             float stepX, float stepY,
                             std::vector<int> &materials,
                             int defaultFluidMaterial = 1,
                             int defaultSolidMaterial = 2);

} // namespace bgi
