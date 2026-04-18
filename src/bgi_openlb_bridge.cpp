#include "bgi_openlb_bridge.h"

#include "bgi_state.h"
#include "bgi_ucs.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <glm/gtc/matrix_transform.hpp>

namespace bgi
{
namespace
{

constexpr float kEpsilon = 1e-4f;

std::string toLower(std::string_view input)
{
    std::string out;
    out.reserve(input.size());
    for (char ch : input)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    return out;
}

std::optional<int> parseIntAttr(const DdsObject &obj, std::string_view key)
{
    auto it = obj.externalAttributes.values.find(std::string(key));
    if (it == obj.externalAttributes.values.end() || it->second.empty())
        return std::nullopt;

    int value = 0;
    const char *begin = it->second.data();
    const char *end = begin + it->second.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        return std::nullopt;
    return value;
}

std::string getAttr(const DdsObject &obj, std::string_view key)
{
    auto it = obj.externalAttributes.values.find(std::string(key));
    return (it != obj.externalAttributes.values.end()) ? it->second : std::string{};
}

bool hasOpenLbAttrs(const DdsObject &obj)
{
    for (const auto &[key, value] : obj.externalAttributes.values)
    {
        (void) value;
        if (key.rfind("openlb.", 0) == 0)
            return true;
    }
    return false;
}

bool isEnabledForOpenLb(const DdsObject &obj)
{
    const std::string enabled = toLower(getAttr(obj, "openlb.enabled"));
    return enabled.empty() || (enabled != "0" && enabled != "false" && enabled != "off");
}

glm::vec3 resolveSolidOriginForBridge(const DdsSolid3D &solid)
{
    if (solid.coordSpace == CoordSpace::UcsLocal)
    {
        auto it = gState.ucsSystems.find(solid.ucsName);
        if (it != gState.ucsSystems.end())
        {
            float wx = 0.f, wy = 0.f, wz = 0.f;
            ucsLocalToWorld(it->second->ucs,
                            solid.origin.x, solid.origin.y, solid.origin.z,
                            wx, wy, wz);
            return {wx, wy, wz};
        }
    }
    return solid.origin;
}

glm::vec3 worldToLeafLocal(const DdsSolid3D &solid,
                           const glm::vec3 &worldPoint,
                           const glm::mat4 &accumulatedTransform)
{
    const glm::vec3 origin = resolveSolidOriginForBridge(solid);
    const glm::mat4 localToWorld = accumulatedTransform *
                                   glm::translate(glm::mat4(1.f), origin);
    const glm::mat4 worldToLocal = glm::inverse(localToWorld);
    const glm::vec4 local = worldToLocal * glm::vec4(worldPoint, 1.f);
    return {local.x, local.y, local.z};
}

bool leafContainsPoint(const DdsObject &obj,
                       const glm::vec3 &worldPoint,
                       const glm::mat4 &accumulatedTransform)
{
    switch (obj.type)
    {
    case DdsObjectType::Box: {
        const auto &box = static_cast<const DdsBox &>(obj);
        const glm::vec3 p = worldToLeafLocal(box, worldPoint, accumulatedTransform);
        return std::abs(p.x) <= box.width * 0.5f + kEpsilon &&
               std::abs(p.y) <= box.depth * 0.5f + kEpsilon &&
               std::abs(p.z) <= box.height * 0.5f + kEpsilon;
    }
    case DdsObjectType::Sphere: {
        const auto &sphere = static_cast<const DdsSphere &>(obj);
        const glm::vec3 p = worldToLeafLocal(sphere, worldPoint, accumulatedTransform);
        return glm::dot(p, p) <= sphere.radius * sphere.radius + kEpsilon;
    }
    case DdsObjectType::Cylinder: {
        const auto &cyl = static_cast<const DdsCylinder &>(obj);
        if (!cyl.caps)
            return false;
        const glm::vec3 p = worldToLeafLocal(cyl, worldPoint, accumulatedTransform);
        const float hh = cyl.height * 0.5f;
        return (p.x * p.x + p.y * p.y) <= cyl.radius * cyl.radius + kEpsilon &&
               p.z >= -hh - kEpsilon && p.z <= hh + kEpsilon;
    }
    case DdsObjectType::Cone: {
        const auto &cone = static_cast<const DdsCone &>(obj);
        if (!cone.cap)
            return false;
        const glm::vec3 p = worldToLeafLocal(cone, worldPoint, accumulatedTransform);
        if (p.z < -kEpsilon || p.z > cone.height + kEpsilon || cone.height <= 0.f)
            return false;
        const float radiusAtZ = cone.radius * std::max(0.f, 1.f - (p.z / cone.height));
        return (p.x * p.x + p.y * p.y) <= radiusAtZ * radiusAtZ + kEpsilon;
    }
    case DdsObjectType::Torus: {
        const auto &torus = static_cast<const DdsTorus &>(obj);
        const glm::vec3 p = worldToLeafLocal(torus, worldPoint, accumulatedTransform);
        const float radial = std::sqrt(p.x * p.x + p.y * p.y);
        const float tube = radial - torus.majorRadius;
        return tube * tube + p.z * p.z <= torus.minorRadius * torus.minorRadius + kEpsilon;
    }
    default:
        return false;
    }
}

int resolveMaterial(const DdsObject &obj, int defaultFluidMaterial, int defaultSolidMaterial)
{
    if (const auto material = parseIntAttr(obj, "openlb.material"))
        return *material;

    const std::string role = toLower(getAttr(obj, "openlb.role"));
    if (role == "fluid")
        return defaultFluidMaterial;
    return defaultSolidMaterial;
}

void applyObjectMetadata(const DdsObject &obj,
                         OpenLbMaterialHit &hit,
                         int defaultFluidMaterial,
                         int defaultSolidMaterial)
{
    hit.matched = true;
    hit.objectId = obj.id;
    hit.role = toLower(getAttr(obj, "openlb.role"));
    hit.boundary = getAttr(obj, "openlb.boundary");
    hit.priority = parseIntAttr(obj, "openlb.priority").value_or(hit.priority);
    hit.material = resolveMaterial(obj, defaultFluidMaterial, defaultSolidMaterial);
    hit.occupied = hit.role != "fluid";
}

bool classifyNodeRecursive(const DdsScene &scene,
                           const DdsObject &obj,
                           const glm::vec3 &worldPoint,
                           const glm::mat4 &transform,
                           OpenLbMaterialHit &hit,
                           int defaultFluidMaterial,
                           int defaultSolidMaterial,
                           std::unordered_set<std::string> &visiting);

bool classifyChildById(const DdsScene &scene,
                       const std::string &childId,
                       const glm::vec3 &worldPoint,
                       const glm::mat4 &transform,
                       OpenLbMaterialHit &hit,
                       int defaultFluidMaterial,
                       int defaultSolidMaterial,
                       std::unordered_set<std::string> &visiting)
{
    auto child = scene.findById(childId);
    if (!child)
        return false;
    return classifyNodeRecursive(scene, *child, worldPoint, transform, hit,
                                 defaultFluidMaterial, defaultSolidMaterial, visiting);
}

bool classifyNodeRecursive(const DdsScene &scene,
                           const DdsObject &obj,
                           const glm::vec3 &worldPoint,
                           const glm::mat4 &transform,
                           OpenLbMaterialHit &hit,
                           int defaultFluidMaterial,
                           int defaultSolidMaterial,
                           std::unordered_set<std::string> &visiting)
{
    if (obj.deleted || !obj.visible || !isEnabledForOpenLb(obj))
        return false;
    if (!visiting.insert(obj.id).second)
        return false;

    const auto eraseGuard = [&]() { visiting.erase(obj.id); };

    switch (obj.type)
    {
    case DdsObjectType::Transform: {
        const auto &tx = static_cast<const DdsTransform &>(obj);
        const glm::mat4 nextTransform = transform * tx.matrix;
        for (const auto &childId : tx.children)
        {
            OpenLbMaterialHit childHit;
            if (!classifyChildById(scene, childId, worldPoint, nextTransform, childHit,
                                   defaultFluidMaterial, defaultSolidMaterial, visiting))
                continue;
            hit = childHit;
            if (hasOpenLbAttrs(obj))
                applyObjectMetadata(obj, hit, defaultFluidMaterial, defaultSolidMaterial);
            eraseGuard();
            return true;
        }
        eraseGuard();
        return false;
    }
    case DdsObjectType::SetUnion: {
        const auto &setObj = static_cast<const DdsSetUnion &>(obj);
        for (const auto &operandId : setObj.operands)
        {
            OpenLbMaterialHit operandHit;
            if (!classifyChildById(scene, operandId, worldPoint, transform, operandHit,
                                   defaultFluidMaterial, defaultSolidMaterial, visiting))
                continue;
            hit = operandHit;
            if (hasOpenLbAttrs(obj))
                applyObjectMetadata(obj, hit, defaultFluidMaterial, defaultSolidMaterial);
            eraseGuard();
            return true;
        }
        eraseGuard();
        return false;
    }
    case DdsObjectType::SetIntersection: {
        const auto &setObj = static_cast<const DdsSetIntersection &>(obj);
        if (setObj.operands.empty())
        {
            eraseGuard();
            return false;
        }

        OpenLbMaterialHit baseHit;
        if (!classifyChildById(scene, setObj.operands.front(), worldPoint, transform, baseHit,
                               defaultFluidMaterial, defaultSolidMaterial, visiting))
        {
            eraseGuard();
            return false;
        }

        for (std::size_t i = 1; i < setObj.operands.size(); ++i)
        {
            OpenLbMaterialHit operandHit;
            if (!classifyChildById(scene, setObj.operands[i], worldPoint, transform, operandHit,
                                   defaultFluidMaterial, defaultSolidMaterial, visiting))
            {
                eraseGuard();
                return false;
            }
        }

        hit = baseHit;
        if (hasOpenLbAttrs(obj))
            applyObjectMetadata(obj, hit, defaultFluidMaterial, defaultSolidMaterial);
        eraseGuard();
        return true;
    }
    case DdsObjectType::SetDifference: {
        const auto &setObj = static_cast<const DdsSetDifference &>(obj);
        if (setObj.operands.empty())
        {
            eraseGuard();
            return false;
        }

        OpenLbMaterialHit baseHit;
        if (!classifyChildById(scene, setObj.operands.front(), worldPoint, transform, baseHit,
                               defaultFluidMaterial, defaultSolidMaterial, visiting))
        {
            eraseGuard();
            return false;
        }

        for (std::size_t i = 1; i < setObj.operands.size(); ++i)
        {
            OpenLbMaterialHit operandHit;
            if (classifyChildById(scene, setObj.operands[i], worldPoint, transform, operandHit,
                                  defaultFluidMaterial, defaultSolidMaterial, visiting))
            {
                eraseGuard();
                return false;
            }
        }

        hit = baseHit;
        if (hasOpenLbAttrs(obj))
            applyObjectMetadata(obj, hit, defaultFluidMaterial, defaultSolidMaterial);
        eraseGuard();
        return true;
    }
    case DdsObjectType::Box:
    case DdsObjectType::Sphere:
    case DdsObjectType::Cylinder:
    case DdsObjectType::Cone:
    case DdsObjectType::Torus:
        if (leafContainsPoint(obj, worldPoint, transform))
        {
            applyObjectMetadata(obj, hit, defaultFluidMaterial, defaultSolidMaterial);
            eraseGuard();
            return true;
        }
        eraseGuard();
        return false;
    default:
        eraseGuard();
        return false;
    }
}

} // namespace

bool openlbClassifyPoint(const DdsScene &scene,
                         const glm::vec3 &worldPoint,
                         OpenLbMaterialHit &hit,
                         int defaultFluidMaterial,
                         int defaultSolidMaterial)
{
    hit = OpenLbMaterialHit{};
    std::unordered_set<std::string> visiting;

    scene.forEachRenderRoot([&](const DdsObject &obj) {
        OpenLbMaterialHit candidate;
        if (!classifyNodeRecursive(scene, obj, worldPoint, glm::mat4(1.f), candidate,
                                   defaultFluidMaterial, defaultSolidMaterial, visiting))
            return;

        if (!hit.matched ||
            candidate.priority > hit.priority ||
            (candidate.priority == hit.priority))
        {
            hit = candidate;
        }
    });

    return hit.matched;
}

void openlbSampleMaterials2D(const DdsScene &scene,
                             float minX, float minY, float z,
                             int cols, int rows,
                             float stepX, float stepY,
                             std::vector<int> &materials,
                             int defaultFluidMaterial,
                             int defaultSolidMaterial)
{
    materials.assign(static_cast<std::size_t>(std::max(0, cols * rows)), defaultFluidMaterial);
    if (cols <= 0 || rows <= 0)
        return;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const glm::vec3 point(minX + (static_cast<float>(col) + 0.5f) * stepX,
                                  minY + (static_cast<float>(row) + 0.5f) * stepY,
                                  z);
            OpenLbMaterialHit hit;
            if (openlbClassifyPoint(scene, point, hit, defaultFluidMaterial, defaultSolidMaterial))
                materials[static_cast<std::size_t>(row * cols + col)] = hit.material;
        }
    }
}

} // namespace bgi
