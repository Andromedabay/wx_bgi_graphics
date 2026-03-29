/**
 * @file bgi_dds_serial.cpp
 * @brief JSON (DDJ) and YAML (DDY) serialization for the DDS scene graph.
 *
 * JSON serialization is handled by nlohmann/json.
 * YAML serialization is handled by yaml-cpp.
 *
 * Both formats represent the same schema:
 *   version          — integer (always 1)
 *   activeCamera     — string
 *   activeUcs        — string
 *   worldExtents     — AABB object (hasData, min[3], max[3])
 *   objects          — array of object records
 *
 * Each object record contains:
 *   id, label, type (string name), visible
 *   Plus type-specific fields (e.g. camera data, UCS axes, draw coords/style).
 *
 * On load, cameras and UCS objects are reconstructed into BgiState::cameras /
 * ucsSystems in addition to the DDS scene.
 */

#include "wx_bgi_dds.h"

#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include "bgi_dds.h"
#include "bgi_state.h"

using json = nlohmann::json;

// =============================================================================
// Coordinate-space string constants
// =============================================================================

namespace
{

constexpr int kDdsVersion = 1;

// ---------------------------------------------------------------------------
// String ↔ enum helpers
// ---------------------------------------------------------------------------

const char *coordSpaceStr(bgi::CoordSpace cs)
{
    switch (cs)
    {
        case bgi::CoordSpace::BgiPixel: return "BgiPixel";
        case bgi::CoordSpace::World3D:  return "World3D";
        case bgi::CoordSpace::UcsLocal: return "UcsLocal";
        default:                        return "BgiPixel";
    }
}

bgi::CoordSpace coordSpaceFromStr(const std::string &s)
{
    if (s == "World3D")  return bgi::CoordSpace::World3D;
    if (s == "UcsLocal") return bgi::CoordSpace::UcsLocal;
    return bgi::CoordSpace::BgiPixel;
}

const char *typeStr(bgi::DdsObjectType t)
{
    switch (t)
    {
        case bgi::DdsObjectType::Camera:          return "Camera";
        case bgi::DdsObjectType::Ucs:             return "Ucs";
        case bgi::DdsObjectType::WorldExtentsObj: return "WorldExtents";
        case bgi::DdsObjectType::Point:           return "Point";
        case bgi::DdsObjectType::Line:            return "Line";
        case bgi::DdsObjectType::Circle:          return "Circle";
        case bgi::DdsObjectType::Arc:             return "Arc";
        case bgi::DdsObjectType::Ellipse:         return "Ellipse";
        case bgi::DdsObjectType::FillEllipse:     return "FillEllipse";
        case bgi::DdsObjectType::PieSlice:        return "PieSlice";
        case bgi::DdsObjectType::Sector:          return "Sector";
        case bgi::DdsObjectType::Rectangle:       return "Rectangle";
        case bgi::DdsObjectType::Bar:             return "Bar";
        case bgi::DdsObjectType::Bar3D:           return "Bar3D";
        case bgi::DdsObjectType::Polygon:         return "Polygon";
        case bgi::DdsObjectType::FillPoly:        return "FillPoly";
        case bgi::DdsObjectType::Text:            return "Text";
        case bgi::DdsObjectType::Image:           return "Image";
        case bgi::DdsObjectType::Box:             return "Box";
        case bgi::DdsObjectType::Sphere:          return "Sphere";
        case bgi::DdsObjectType::Cylinder:        return "Cylinder";
        case bgi::DdsObjectType::Cone:            return "Cone";
        case bgi::DdsObjectType::Torus:           return "Torus";
        case bgi::DdsObjectType::HeightMap:       return "HeightMap";
        case bgi::DdsObjectType::ParamSurface:    return "ParamSurface";
        case bgi::DdsObjectType::Extrusion:       return "Extrusion";
        default:                                  return "Unknown";
    }
}

const char *projStr(bgi::CameraProjection p)
{
    return (p == bgi::CameraProjection::Perspective) ? "Perspective" : "Orthographic";
}

bgi::CameraProjection projFromStr(const std::string &s)
{
    return (s == "Perspective") ? bgi::CameraProjection::Perspective
                                : bgi::CameraProjection::Orthographic;
}

// ---------------------------------------------------------------------------
// glm::vec3 helpers
// ---------------------------------------------------------------------------

json vec3ToJ(const glm::vec3 &v)    { return json::array({v.x, v.y, v.z}); }
json vec2ToJ(float x, float y)       { return json::array({x, y}); }
json vec4ToJ(float a, float b,
              float c, float d)      { return json::array({a, b, c, d}); }

glm::vec3 vec3FromJ(const json &j)
{
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

// ---------------------------------------------------------------------------
// Style helpers
// ---------------------------------------------------------------------------

json styleToJ(const bgi::DdsStyle &s)
{
    return {
        {"color",     s.color},
        {"writeMode", s.writeMode},
        {"bkColor",   s.bkColor},
        {"lineStyle", {
            {"linestyle",  s.lineStyle.linestyle},
            {"upattern",   s.lineStyle.upattern},
            {"thickness",  s.lineStyle.thickness}
        }},
        {"fillStyle", {
            {"pattern", s.fillStyle.pattern},
            {"color",   s.fillStyle.color}
        }},
        {"textStyle", {
            {"font",      s.textStyle.font},
            {"direction", s.textStyle.direction},
            {"charsize",  s.textStyle.charsize},
            {"horiz",     s.textStyle.horiz},
            {"vert",      s.textStyle.vert}
        }}
    };
}

bgi::DdsStyle styleFromJ(const json &j)
{
    bgi::DdsStyle s;
    s.color     = j.value("color",     0);
    s.writeMode = j.value("writeMode", 0);
    s.bkColor   = j.value("bkColor",   0);
    if (j.contains("lineStyle"))
    {
        const auto &ls = j["lineStyle"];
        s.lineStyle.linestyle = ls.value("linestyle", 0);
        s.lineStyle.upattern  = ls.value("upattern",  0xFFFFU);
        s.lineStyle.thickness = ls.value("thickness", 1);
    }
    if (j.contains("fillStyle"))
    {
        const auto &fs = j["fillStyle"];
        s.fillStyle.pattern = fs.value("pattern", 1);
        s.fillStyle.color   = fs.value("color",   0);
    }
    if (j.contains("textStyle"))
    {
        const auto &ts = j["textStyle"];
        s.textStyle.font      = ts.value("font",      0);
        s.textStyle.direction = ts.value("direction", 0);
        s.textStyle.charsize  = ts.value("charsize",  1);
        s.textStyle.horiz     = ts.value("horiz",     0);
        s.textStyle.vert      = ts.value("vert",      2);
    }
    return s;
}

// ---------------------------------------------------------------------------
// Base object header (id, label, type, visible)
// ---------------------------------------------------------------------------

void baseObjToJ(json &j, const bgi::DdsObject &o)
{
    j["id"]      = o.id;
    j["label"]   = o.label;
    j["type"]    = typeStr(o.type);
    j["visible"] = o.visible;
}

// ---------------------------------------------------------------------------
// Camera serialization
// ---------------------------------------------------------------------------

json cameraToJ(const bgi::DdsCamera &dc)
{
    json j;
    baseObjToJ(j, dc);
    j["name"]         = dc.name;
    j["projection"]   = projStr(dc.camera.projection);
    j["eye"]          = vec3ToJ({dc.camera.eyeX, dc.camera.eyeY, dc.camera.eyeZ});
    j["target"]       = vec3ToJ({dc.camera.targetX, dc.camera.targetY, dc.camera.targetZ});
    j["up"]           = vec3ToJ({dc.camera.upX, dc.camera.upY, dc.camera.upZ});
    j["fovY"]         = dc.camera.fovYDeg;
    j["near"]         = dc.camera.nearPlane;
    j["far"]          = dc.camera.farPlane;
    j["ortho"]        = vec4ToJ(dc.camera.orthoLeft, dc.camera.orthoRight,
                                dc.camera.orthoBottom, dc.camera.orthoTop);
    j["viewport"]     = json::array({dc.camera.vpX, dc.camera.vpY, dc.camera.vpW, dc.camera.vpH});
    j["is2D"]         = dc.camera.is2D;
    j["pan2d"]        = vec2ToJ(dc.camera.pan2dX, dc.camera.pan2dY);
    j["zoom2d"]       = dc.camera.zoom2d;
    j["rot2d"]        = dc.camera.rot2dDeg;
    j["worldHeight2d"]= dc.camera.worldHeight2d;
    return j;
}

std::shared_ptr<bgi::DdsCamera> cameraFromJ(const json &j)
{
    auto dc = std::make_shared<bgi::DdsCamera>();
    dc->id              = j.value("id",    "");
    dc->label           = j.value("label", "");
    dc->visible         = j.value("visible", true);
    dc->name            = j.value("name",  "");
    dc->camera.projection = projFromStr(j.value("projection", "Orthographic"));
    if (j.contains("eye")) {
        auto v = vec3FromJ(j["eye"]);
        dc->camera.eyeX = v.x; dc->camera.eyeY = v.y; dc->camera.eyeZ = v.z;
    }
    if (j.contains("target")) {
        auto v = vec3FromJ(j["target"]);
        dc->camera.targetX = v.x; dc->camera.targetY = v.y; dc->camera.targetZ = v.z;
    }
    if (j.contains("up")) {
        auto v = vec3FromJ(j["up"]);
        dc->camera.upX = v.x; dc->camera.upY = v.y; dc->camera.upZ = v.z;
    }
    dc->camera.fovYDeg     = j.value("fovY",  45.f);
    dc->camera.nearPlane   = j.value("near",  -1.f);
    dc->camera.farPlane    = j.value("far",   1.f);
    if (j.contains("ortho") && j["ortho"].size() >= 4) {
        dc->camera.orthoLeft   = j["ortho"][0];
        dc->camera.orthoRight  = j["ortho"][1];
        dc->camera.orthoBottom = j["ortho"][2];
        dc->camera.orthoTop    = j["ortho"][3];
    }
    if (j.contains("viewport") && j["viewport"].size() >= 4) {
        dc->camera.vpX = j["viewport"][0];
        dc->camera.vpY = j["viewport"][1];
        dc->camera.vpW = j["viewport"][2];
        dc->camera.vpH = j["viewport"][3];
    }
    dc->camera.is2D        = j.value("is2D",          false);
    if (j.contains("pan2d") && j["pan2d"].size() >= 2) {
        dc->camera.pan2dX = j["pan2d"][0];
        dc->camera.pan2dY = j["pan2d"][1];
    }
    dc->camera.zoom2d       = j.value("zoom2d",        1.f);
    dc->camera.rot2dDeg     = j.value("rot2d",         0.f);
    dc->camera.worldHeight2d= j.value("worldHeight2d", 2.f);
    return dc;
}

// ---------------------------------------------------------------------------
// UCS serialization
// ---------------------------------------------------------------------------

json ucsToJ(const bgi::DdsUcs &du)
{
    json j;
    baseObjToJ(j, du);
    j["name"]   = du.name;
    j["origin"] = vec3ToJ({du.ucs.originX, du.ucs.originY, du.ucs.originZ});
    j["xAxis"]  = vec3ToJ({du.ucs.xAxisX,  du.ucs.xAxisY,  du.ucs.xAxisZ});
    j["yAxis"]  = vec3ToJ({du.ucs.yAxisX,  du.ucs.yAxisY,  du.ucs.yAxisZ});
    j["zAxis"]  = vec3ToJ({du.ucs.zAxisX,  du.ucs.zAxisY,  du.ucs.zAxisZ});
    return j;
}

std::shared_ptr<bgi::DdsUcs> ucsFromJ(const json &j)
{
    auto du = std::make_shared<bgi::DdsUcs>();
    du->id      = j.value("id",    "");
    du->label   = j.value("label", "");
    du->visible = j.value("visible", true);
    du->name    = j.value("name",  "");
    if (j.contains("origin")) {
        auto v = vec3FromJ(j["origin"]);
        du->ucs.originX = v.x; du->ucs.originY = v.y; du->ucs.originZ = v.z;
    }
    if (j.contains("xAxis")) {
        auto v = vec3FromJ(j["xAxis"]);
        du->ucs.xAxisX = v.x; du->ucs.xAxisY = v.y; du->ucs.xAxisZ = v.z;
    }
    if (j.contains("yAxis")) {
        auto v = vec3FromJ(j["yAxis"]);
        du->ucs.yAxisX = v.x; du->ucs.yAxisY = v.y; du->ucs.yAxisZ = v.z;
    }
    if (j.contains("zAxis")) {
        auto v = vec3FromJ(j["zAxis"]);
        du->ucs.zAxisX = v.x; du->ucs.zAxisY = v.y; du->ucs.zAxisZ = v.z;
    }
    return du;
}

// ---------------------------------------------------------------------------
// WorldExtents serialization
// ---------------------------------------------------------------------------

json extentsToJ(const bgi::DdsWorldExtentsObj &we)
{
    json j;
    baseObjToJ(j, we);
    j["hasData"] = we.extents.hasData;
    j["min"]     = vec3ToJ({we.extents.minX, we.extents.minY, we.extents.minZ});
    j["max"]     = vec3ToJ({we.extents.maxX, we.extents.maxY, we.extents.maxZ});
    return j;
}

std::shared_ptr<bgi::DdsWorldExtentsObj> extentsFromJ(const json &j)
{
    auto we = std::make_shared<bgi::DdsWorldExtentsObj>();
    we->id      = j.value("id",    "");
    we->label   = j.value("label", "");
    we->visible = j.value("visible", true);
    we->extents.hasData = j.value("hasData", false);
    if (j.contains("min")) {
        auto v = vec3FromJ(j["min"]);
        we->extents.minX = v.x; we->extents.minY = v.y; we->extents.minZ = v.z;
    }
    if (j.contains("max")) {
        auto v = vec3FromJ(j["max"]);
        we->extents.maxX = v.x; we->extents.maxY = v.y; we->extents.maxZ = v.z;
    }
    return we;
}

// ---------------------------------------------------------------------------
// CoordSpace header for drawing objects
// ---------------------------------------------------------------------------

void drawHeaderToJ(json &j, bgi::CoordSpace cs, const std::string &ucsName,
                   const bgi::DdsStyle &style)
{
    j["coordSpace"] = coordSpaceStr(cs);
    j["ucsName"]    = ucsName;
    j["style"]      = styleToJ(style);
}

// ---------------------------------------------------------------------------
// Drawing object serialization
// ---------------------------------------------------------------------------

json objectToJ(const bgi::DdsObject &obj)
{
    json j;
    baseObjToJ(j, obj);

    switch (obj.type)
    {
    case bgi::DdsObjectType::Camera:
        return cameraToJ(static_cast<const bgi::DdsCamera&>(obj));

    case bgi::DdsObjectType::Ucs:
        return ucsToJ(static_cast<const bgi::DdsUcs&>(obj));

    case bgi::DdsObjectType::WorldExtentsObj:
        return extentsToJ(static_cast<const bgi::DdsWorldExtentsObj&>(obj));

    case bgi::DdsObjectType::Point: {
        const auto &o = static_cast<const bgi::DdsPoint&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["pos"]   = vec3ToJ(o.pos);
        j["color"] = o.color;
        break;
    }
    case bgi::DdsObjectType::Line: {
        const auto &o = static_cast<const bgi::DdsLine&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["p1"] = vec3ToJ(o.p1);
        j["p2"] = vec3ToJ(o.p2);
        break;
    }
    case bgi::DdsObjectType::Circle: {
        const auto &o = static_cast<const bgi::DdsCircle&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"] = vec3ToJ(o.centre);
        j["radius"] = o.radius;
        break;
    }
    case bgi::DdsObjectType::Arc: {
        const auto &o = static_cast<const bgi::DdsArc&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"]     = vec3ToJ(o.centre);
        j["radius"]     = o.radius;
        j["startAngle"] = o.startAngle;
        j["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Ellipse: {
        const auto &o = static_cast<const bgi::DdsEllipse&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"]     = vec3ToJ(o.centre);
        j["rx"]         = o.rx;
        j["ry"]         = o.ry;
        j["startAngle"] = o.startAngle;
        j["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::FillEllipse: {
        const auto &o = static_cast<const bgi::DdsFillEllipse&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"] = vec3ToJ(o.centre);
        j["rx"]     = o.rx;
        j["ry"]     = o.ry;
        break;
    }
    case bgi::DdsObjectType::PieSlice: {
        const auto &o = static_cast<const bgi::DdsPieSlice&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"]     = vec3ToJ(o.centre);
        j["radius"]     = o.radius;
        j["startAngle"] = o.startAngle;
        j["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Sector: {
        const auto &o = static_cast<const bgi::DdsSector&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["centre"]     = vec3ToJ(o.centre);
        j["rx"]         = o.rx;
        j["ry"]         = o.ry;
        j["startAngle"] = o.startAngle;
        j["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Rectangle: {
        const auto &o = static_cast<const bgi::DdsRectangle&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["p1"] = vec3ToJ(o.p1);
        j["p2"] = vec3ToJ(o.p2);
        break;
    }
    case bgi::DdsObjectType::Bar: {
        const auto &o = static_cast<const bgi::DdsBar&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["p1"] = vec3ToJ(o.p1);
        j["p2"] = vec3ToJ(o.p2);
        break;
    }
    case bgi::DdsObjectType::Bar3D: {
        const auto &o = static_cast<const bgi::DdsBar3D&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["p1"]      = vec3ToJ(o.p1);
        j["p2"]      = vec3ToJ(o.p2);
        j["depth"]   = o.depth;
        j["topFlag"] = o.topFlag;
        break;
    }
    case bgi::DdsObjectType::Polygon: {
        const auto &o = static_cast<const bgi::DdsPolygon&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        auto pts = json::array();
        for (const auto &p : o.pts) pts.push_back(vec3ToJ(p));
        j["pts"] = pts;
        break;
    }
    case bgi::DdsObjectType::FillPoly: {
        const auto &o = static_cast<const bgi::DdsFillPoly&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        auto pts = json::array();
        for (const auto &p : o.pts) pts.push_back(vec3ToJ(p));
        j["pts"] = pts;
        break;
    }
    case bgi::DdsObjectType::Text: {
        const auto &o = static_cast<const bgi::DdsText&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["pos"]  = vec3ToJ(o.pos);
        j["text"] = o.text;
        break;
    }
    case bgi::DdsObjectType::Image: {
        const auto &o = static_cast<const bgi::DdsImage&>(obj);
        j["coordSpace"] = coordSpaceStr(o.coordSpace);
        j["style"]      = styleToJ(o.style);
        j["pos"]        = vec3ToJ(o.pos);
        j["width"]      = o.width;
        j["height"]     = o.height;
        // Store pixels as base64 is complex — store as hex string for now.
        // Empty pixels are represented as "".
        j["pixels"]     = json::binary(o.pixels);
        break;
    }
    // Phase 4/5/6 — shared solid base fields
    case bgi::DdsObjectType::Box: {
        const auto &o = static_cast<const bgi::DdsBox&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]    = vec3ToJ(o.origin);
        j["drawMode"]  = static_cast<int>(o.drawMode);
        j["edgeColor"] = o.edgeColor;
        j["faceColor"] = o.faceColor;
        j["slices"]    = o.slices;
        j["stacks"]    = o.stacks;
        j["width"]     = o.width;
        j["depth"]     = o.depth;
        j["height"]    = o.height;
        break;
    }
    case bgi::DdsObjectType::Sphere: {
        const auto &o = static_cast<const bgi::DdsSphere&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]    = vec3ToJ(o.origin);
        j["drawMode"]  = static_cast<int>(o.drawMode);
        j["edgeColor"] = o.edgeColor;
        j["faceColor"] = o.faceColor;
        j["slices"]    = o.slices;
        j["stacks"]    = o.stacks;
        j["radius"]    = o.radius;
        break;
    }
    case bgi::DdsObjectType::Cylinder: {
        const auto &o = static_cast<const bgi::DdsCylinder&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]    = vec3ToJ(o.origin);
        j["drawMode"]  = static_cast<int>(o.drawMode);
        j["edgeColor"] = o.edgeColor;
        j["faceColor"] = o.faceColor;
        j["slices"]    = o.slices;
        j["stacks"]    = o.stacks;
        j["radius"]    = o.radius;
        j["height"]    = o.height;
        j["caps"]      = o.caps;
        break;
    }
    case bgi::DdsObjectType::Cone: {
        const auto &o = static_cast<const bgi::DdsCone&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]    = vec3ToJ(o.origin);
        j["drawMode"]  = static_cast<int>(o.drawMode);
        j["edgeColor"] = o.edgeColor;
        j["faceColor"] = o.faceColor;
        j["slices"]    = o.slices;
        j["stacks"]    = o.stacks;
        j["radius"]    = o.radius;
        j["height"]    = o.height;
        j["cap"]       = o.cap;
        break;
    }
    case bgi::DdsObjectType::Torus: {
        const auto &o = static_cast<const bgi::DdsTorus&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]      = vec3ToJ(o.origin);
        j["drawMode"]    = static_cast<int>(o.drawMode);
        j["edgeColor"]   = o.edgeColor;
        j["faceColor"]   = o.faceColor;
        j["slices"]      = o.slices;
        j["stacks"]      = o.stacks;
        j["majorRadius"] = o.majorRadius;
        j["minorRadius"] = o.minorRadius;
        break;
    }
    case bgi::DdsObjectType::HeightMap: {
        const auto &o = static_cast<const bgi::DdsHeightMap&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]     = vec3ToJ(o.origin);
        j["drawMode"]   = static_cast<int>(o.drawMode);
        j["edgeColor"]  = o.edgeColor;
        j["faceColor"]  = o.faceColor;
        j["slices"]     = o.slices;
        j["stacks"]     = o.stacks;
        j["rows"]       = o.rows;
        j["cols"]       = o.cols;
        j["cellWidth"]  = o.cellWidth;
        j["cellHeight"] = o.cellHeight;
        {
            auto ha = json::array();
            for (float h : o.heights) ha.push_back(h);
            j["heights"] = ha;
        }
        break;
    }
    case bgi::DdsObjectType::ParamSurface: {
        const auto &o = static_cast<const bgi::DdsParamSurface&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]    = vec3ToJ(o.origin);
        j["drawMode"]  = static_cast<int>(o.drawMode);
        j["edgeColor"] = o.edgeColor;
        j["faceColor"] = o.faceColor;
        j["slices"]    = o.slices;
        j["stacks"]    = o.stacks;
        j["formula"]   = static_cast<int>(o.formula);
        j["param1"]    = o.param1;
        j["param2"]    = o.param2;
        j["uSteps"]    = o.uSteps;
        j["vSteps"]    = o.vSteps;
        break;
    }
    case bgi::DdsObjectType::Extrusion: {
        const auto &o = static_cast<const bgi::DdsExtrusion&>(obj);
        drawHeaderToJ(j, o.coordSpace, o.ucsName, o.style);
        j["origin"]     = vec3ToJ(o.origin);
        j["drawMode"]   = static_cast<int>(o.drawMode);
        j["edgeColor"]  = o.edgeColor;
        j["faceColor"]  = o.faceColor;
        j["slices"]     = o.slices;
        j["stacks"]     = o.stacks;
        j["extrudeDir"] = vec3ToJ(o.extrudeDir);
        j["capStart"]   = o.capStart;
        j["capEnd"]     = o.capEnd;
        {
            auto pts = json::array();
            for (const auto &p : o.baseProfile) pts.push_back(vec3ToJ(p));
            j["pts"] = pts;
        }
        break;
    }
    default:
        break;
    }
    return j;
}

// ---------------------------------------------------------------------------
// Drawing object deserialization helper
// ---------------------------------------------------------------------------

std::shared_ptr<bgi::DdsObject> objectFromJ(const json &j)
{
    const std::string typeStr = j.value("type", "");

    // Config objects
    if (typeStr == "Camera")        return cameraFromJ(j);
    if (typeStr == "Ucs")           return ucsFromJ(j);
    if (typeStr == "WorldExtents")  return extentsFromJ(j);

    // Drawing objects — parse common draw header first
    auto getCS  = [&]() { return coordSpaceFromStr(j.value("coordSpace", "BgiPixel")); };
    auto getUcs = [&]() { return j.value("ucsName", std::string{}); };
    auto getSty = [&]() { return j.contains("style") ? styleFromJ(j["style"]) : bgi::DdsStyle{}; };

    if (typeStr == "Point") {
        auto o = std::make_shared<bgi::DdsPoint>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("pos"))   o->pos   = vec3FromJ(j["pos"]);
        o->color = j.value("color", 0);
        return o;
    }
    if (typeStr == "Line") {
        auto o = std::make_shared<bgi::DdsLine>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("p1")) o->p1 = vec3FromJ(j["p1"]);
        if (j.contains("p2")) o->p2 = vec3FromJ(j["p2"]);
        return o;
    }
    if (typeStr == "Circle") {
        auto o = std::make_shared<bgi::DdsCircle>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->radius = j.value("radius", 0.f);
        return o;
    }
    if (typeStr == "Arc") {
        auto o = std::make_shared<bgi::DdsArc>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->radius     = j.value("radius",     0.f);
        o->startAngle = j.value("startAngle", 0.f);
        o->endAngle   = j.value("endAngle",   0.f);
        return o;
    }
    if (typeStr == "Ellipse") {
        auto o = std::make_shared<bgi::DdsEllipse>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->rx = j.value("rx", 0.f); o->ry = j.value("ry", 0.f);
        o->startAngle = j.value("startAngle", 0.f);
        o->endAngle   = j.value("endAngle",   0.f);
        return o;
    }
    if (typeStr == "FillEllipse") {
        auto o = std::make_shared<bgi::DdsFillEllipse>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->rx = j.value("rx", 0.f); o->ry = j.value("ry", 0.f);
        return o;
    }
    if (typeStr == "PieSlice") {
        auto o = std::make_shared<bgi::DdsPieSlice>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->radius     = j.value("radius",     0.f);
        o->startAngle = j.value("startAngle", 0.f);
        o->endAngle   = j.value("endAngle",   0.f);
        return o;
    }
    if (typeStr == "Sector") {
        auto o = std::make_shared<bgi::DdsSector>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("centre")) o->centre = vec3FromJ(j["centre"]);
        o->rx = j.value("rx", 0.f); o->ry = j.value("ry", 0.f);
        o->startAngle = j.value("startAngle", 0.f);
        o->endAngle   = j.value("endAngle",   0.f);
        return o;
    }
    if (typeStr == "Rectangle") {
        auto o = std::make_shared<bgi::DdsRectangle>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("p1")) o->p1 = vec3FromJ(j["p1"]);
        if (j.contains("p2")) o->p2 = vec3FromJ(j["p2"]);
        return o;
    }
    if (typeStr == "Bar") {
        auto o = std::make_shared<bgi::DdsBar>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("p1")) o->p1 = vec3FromJ(j["p1"]);
        if (j.contains("p2")) o->p2 = vec3FromJ(j["p2"]);
        return o;
    }
    if (typeStr == "Bar3D") {
        auto o = std::make_shared<bgi::DdsBar3D>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("p1")) o->p1 = vec3FromJ(j["p1"]);
        if (j.contains("p2")) o->p2 = vec3FromJ(j["p2"]);
        o->depth   = j.value("depth",   0.f);
        o->topFlag = j.value("topFlag", true);
        return o;
    }
    if (typeStr == "Polygon") {
        auto o = std::make_shared<bgi::DdsPolygon>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("pts"))
            for (const auto &p : j["pts"]) o->pts.push_back(vec3FromJ(p));
        return o;
    }
    if (typeStr == "FillPoly") {
        auto o = std::make_shared<bgi::DdsFillPoly>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("pts"))
            for (const auto &p : j["pts"]) o->pts.push_back(vec3FromJ(p));
        return o;
    }
    if (typeStr == "Text") {
        auto o = std::make_shared<bgi::DdsText>();
        o->coordSpace = getCS(); o->ucsName = getUcs(); o->style = getSty();
        if (j.contains("pos")) o->pos = vec3FromJ(j["pos"]);
        o->text = j.value("text", std::string{});
        return o;
    }
    if (typeStr == "Image") {
        auto o = std::make_shared<bgi::DdsImage>();
        o->coordSpace = getCS(); o->style = getSty();
        if (j.contains("pos")) o->pos = vec3FromJ(j["pos"]);
        o->width  = j.value("width",  0);
        o->height = j.value("height", 0);
        if (j.contains("pixels") && j["pixels"].is_binary())
            o->pixels = j["pixels"].get_binary();
        return o;
    }

    // Phase 4/5/6 — solid base field loader (lambda)
    auto loadSolidBase = [&](bgi::DdsSolid3D &s) {
        s.coordSpace = getCS(); s.ucsName = getUcs(); s.style = getSty();
        if (j.contains("origin")) s.origin = vec3FromJ(j["origin"]);
        s.drawMode  = static_cast<bgi::SolidDrawMode>(j.value("drawMode",  0));
        s.edgeColor = j.value("edgeColor", 15);
        s.faceColor = j.value("faceColor",  7);
        s.slices    = j.value("slices",    16);
        s.stacks    = j.value("stacks",     8);
    };

    if (typeStr == "Box") {
        auto o = std::make_shared<bgi::DdsBox>();
        loadSolidBase(*o);
        o->width  = j.value("width",  1.f);
        o->depth  = j.value("depth",  1.f);
        o->height = j.value("height", 1.f);
        return o;
    }
    if (typeStr == "Sphere") {
        auto o = std::make_shared<bgi::DdsSphere>();
        loadSolidBase(*o);
        o->radius = j.value("radius", 1.f);
        return o;
    }
    if (typeStr == "Cylinder") {
        auto o = std::make_shared<bgi::DdsCylinder>();
        loadSolidBase(*o);
        o->radius = j.value("radius", 1.f);
        o->height = j.value("height", 1.f);
        o->caps   = j.value("caps",   1);
        return o;
    }
    if (typeStr == "Cone") {
        auto o = std::make_shared<bgi::DdsCone>();
        loadSolidBase(*o);
        o->radius = j.value("radius", 1.f);
        o->height = j.value("height", 1.f);
        o->cap    = j.value("cap",    1);
        return o;
    }
    if (typeStr == "Torus") {
        auto o = std::make_shared<bgi::DdsTorus>();
        loadSolidBase(*o);
        o->majorRadius = j.value("majorRadius", 2.f);
        o->minorRadius = j.value("minorRadius", 0.5f);
        return o;
    }
    if (typeStr == "HeightMap") {
        auto o = std::make_shared<bgi::DdsHeightMap>();
        loadSolidBase(*o);
        o->rows       = j.value("rows",       0);
        o->cols       = j.value("cols",       0);
        o->cellWidth  = j.value("cellWidth",  1.f);
        o->cellHeight = j.value("cellHeight", 1.f);
        if (j.contains("heights"))
            for (const auto &h : j["heights"]) o->heights.push_back(h.get<float>());
        return o;
    }
    if (typeStr == "ParamSurface") {
        auto o = std::make_shared<bgi::DdsParamSurface>();
        loadSolidBase(*o);
        o->formula = static_cast<bgi::ParamSurfaceFormula>(j.value("formula", 0));
        o->param1  = j.value("param1",  1.f);
        o->param2  = j.value("param2",  0.5f);
        o->uSteps  = j.value("uSteps",  20);
        o->vSteps  = j.value("vSteps",  20);
        return o;
    }
    if (typeStr == "Extrusion") {
        auto o = std::make_shared<bgi::DdsExtrusion>();
        loadSolidBase(*o);
        if (j.contains("extrudeDir")) o->extrudeDir = vec3FromJ(j["extrudeDir"]);
        o->capStart = j.value("capStart", 1);
        o->capEnd   = j.value("capEnd",   1);
        if (j.contains("pts"))
            for (const auto &p : j["pts"]) o->baseProfile.push_back(vec3FromJ(p));
        return o;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Full scene → JSON  (called while holding gMutex)
// ---------------------------------------------------------------------------

json sceneToJson()
{
    json root;
    root["version"]      = kDdsVersion;
    root["activeCamera"] = bgi::gState.activeCamera;
    root["activeUcs"]    = bgi::gState.activeUcs;

    // World extents
    const auto &we = bgi::gState.worldExtents;
    root["worldExtents"] = {
        {"hasData", we.hasData},
        {"min",     json::array({we.minX, we.minY, we.minZ})},
        {"max",     json::array({we.maxX, we.maxY, we.maxZ})}
    };

    auto objs = json::array();
    bgi::gState.dds->forEach([&](const bgi::DdsObject &obj) {
        objs.push_back(objectToJ(obj));
    });
    root["objects"] = objs;
    return root;
}

// ---------------------------------------------------------------------------
// JSON → full scene  (called while holding gMutex)
// ---------------------------------------------------------------------------

void sceneFromJson(const json &root)
{
    // Clear everything first.
    bgi::gState.dds->clearAll();
    bgi::gState.cameras.clear();
    bgi::gState.ucsSystems.clear();
    bgi::gState.worldExtents = bgi::WorldExtents{};

    // World extents
    if (root.contains("worldExtents")) {
        const auto &we = root["worldExtents"];
        bgi::gState.worldExtents.hasData = we.value("hasData", false);
        if (we.contains("min") && we["min"].size() >= 3) {
            bgi::gState.worldExtents.minX = we["min"][0];
            bgi::gState.worldExtents.minY = we["min"][1];
            bgi::gState.worldExtents.minZ = we["min"][2];
        }
        if (we.contains("max") && we["max"].size() >= 3) {
            bgi::gState.worldExtents.maxX = we["max"][0];
            bgi::gState.worldExtents.maxY = we["max"][1];
            bgi::gState.worldExtents.maxZ = we["max"][2];
        }
    }

    // Objects
    if (root.contains("objects") && root["objects"].is_array()) {
        for (const auto &jo : root["objects"]) {
            auto obj = objectFromJ(jo);
            if (!obj)
                continue;

            // Preserve the serialized id, label, and visibility.
            obj->id      = jo.value("id",      "");
            obj->label   = jo.value("label",   "");
            obj->visible = jo.value("visible", true);

            if (obj->type == bgi::DdsObjectType::Camera) {
                auto dc = std::static_pointer_cast<bgi::DdsCamera>(obj);
                bgi::gState.dds->appendWithId(dc);
                bgi::gState.cameras[dc->name] = dc;
            }
            else if (obj->type == bgi::DdsObjectType::Ucs) {
                auto du = std::static_pointer_cast<bgi::DdsUcs>(obj);
                bgi::gState.dds->appendWithId(du);
                bgi::gState.ucsSystems[du->name] = du;
            }
            else {
                bgi::gState.dds->appendWithId(obj);
            }
        }
    }

    // Restore active camera/UCS (fall back to "default"/"world" if not found).
    bgi::gState.activeCamera = root.value("activeCamera", "default");
    if (!bgi::gState.cameras.count(bgi::gState.activeCamera))
        bgi::gState.activeCamera = bgi::gState.cameras.empty()
                                 ? "default" : bgi::gState.cameras.begin()->first;

    bgi::gState.activeUcs = root.value("activeUcs", "world");
    if (!bgi::gState.ucsSystems.count(bgi::gState.activeUcs))
        bgi::gState.activeUcs = bgi::gState.ucsSystems.empty()
                              ? "world" : bgi::gState.ucsSystems.begin()->first;

    // Ensure mandatory defaults exist even if the JSON omitted them.
    if (!bgi::gState.cameras.count("default")) {
        auto dc = std::make_shared<bgi::DdsCamera>();
        dc->name               = "default";
        dc->camera.projection  = bgi::CameraProjection::Orthographic;
        dc->camera.eyeX        = 0.f; dc->camera.eyeY = 0.f; dc->camera.eyeZ = 1.f;
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
    if (!bgi::gState.ucsSystems.count("world")) {
        auto du = std::make_shared<bgi::DdsUcs>();
        du->name = "world"; du->ucs = bgi::CoordSystem{};
        bgi::gState.dds->append(du);
        bgi::gState.ucsSystems["world"] = du;
    }
}

// =============================================================================
// YAML helpers — mirrors the JSON schema using yaml-cpp's Node API
// =============================================================================

YAML::Node vec3ToY(const glm::vec3 &v)
{
    YAML::Node n(YAML::NodeType::Sequence);
    n.push_back(v.x); n.push_back(v.y); n.push_back(v.z);
    return n;
}

glm::vec3 vec3FromY(const YAML::Node &n)
{
    return {n[0].as<float>(0.f), n[1].as<float>(0.f), n[2].as<float>(0.f)};
}

YAML::Node styleToY(const bgi::DdsStyle &s)
{
    YAML::Node n;
    n["color"]     = s.color;
    n["writeMode"] = s.writeMode;
    n["bkColor"]   = s.bkColor;
    YAML::Node ls;
    ls["linestyle"] = s.lineStyle.linestyle;
    ls["upattern"]  = s.lineStyle.upattern;
    ls["thickness"] = s.lineStyle.thickness;
    n["lineStyle"]  = ls;
    YAML::Node fs;
    fs["pattern"]  = s.fillStyle.pattern;
    fs["color"]    = s.fillStyle.color;
    n["fillStyle"] = fs;
    YAML::Node ts;
    ts["font"]      = s.textStyle.font;
    ts["direction"] = s.textStyle.direction;
    ts["charsize"]  = s.textStyle.charsize;
    ts["horiz"]     = s.textStyle.horiz;
    ts["vert"]      = s.textStyle.vert;
    n["textStyle"]  = ts;
    return n;
}

bgi::DdsStyle styleFromY(const YAML::Node &n)
{
    bgi::DdsStyle s;
    s.color     = n["color"]    .as<int>(0);
    s.writeMode = n["writeMode"].as<int>(0);
    s.bkColor   = n["bkColor"]  .as<int>(0);
    if (n["lineStyle"]) {
        const auto &ls  = n["lineStyle"];
        s.lineStyle.linestyle = ls["linestyle"].as<int>(0);
        s.lineStyle.upattern  = ls["upattern"] .as<unsigned>(0xFFFFU);
        s.lineStyle.thickness = ls["thickness"].as<int>(1);
    }
    if (n["fillStyle"]) {
        const auto &fs = n["fillStyle"];
        s.fillStyle.pattern = fs["pattern"].as<int>(1);
        s.fillStyle.color   = fs["color"]  .as<int>(0);
    }
    if (n["textStyle"]) {
        const auto &ts = n["textStyle"];
        s.textStyle.font      = ts["font"]     .as<int>(0);
        s.textStyle.direction = ts["direction"].as<int>(0);
        s.textStyle.charsize  = ts["charsize"] .as<int>(1);
        s.textStyle.horiz     = ts["horiz"]    .as<int>(0);
        s.textStyle.vert      = ts["vert"]     .as<int>(2);
    }
    return s;
}

void baseObjToY(YAML::Node &n, const bgi::DdsObject &o)
{
    n["id"]      = o.id;
    n["label"]   = o.label;
    n["type"]    = typeStr(o.type);
    n["visible"] = o.visible;
}

YAML::Node objectToY(const bgi::DdsObject &obj)
{
    YAML::Node n;
    baseObjToY(n, obj);

    switch (obj.type)
    {
    case bgi::DdsObjectType::Camera: {
        const auto &dc = static_cast<const bgi::DdsCamera&>(obj);
        n["name"]          = dc.name;
        n["projection"]    = projStr(dc.camera.projection);
        n["eye"]           = vec3ToY({dc.camera.eyeX, dc.camera.eyeY, dc.camera.eyeZ});
        n["target"]        = vec3ToY({dc.camera.targetX, dc.camera.targetY, dc.camera.targetZ});
        n["up"]            = vec3ToY({dc.camera.upX, dc.camera.upY, dc.camera.upZ});
        n["fovY"]          = dc.camera.fovYDeg;
        n["near"]          = dc.camera.nearPlane;
        n["far"]           = dc.camera.farPlane;
        YAML::Node ortho(YAML::NodeType::Sequence);
        ortho.push_back(dc.camera.orthoLeft);  ortho.push_back(dc.camera.orthoRight);
        ortho.push_back(dc.camera.orthoBottom);ortho.push_back(dc.camera.orthoTop);
        n["ortho"]         = ortho;
        YAML::Node vp(YAML::NodeType::Sequence);
        vp.push_back(dc.camera.vpX); vp.push_back(dc.camera.vpY);
        vp.push_back(dc.camera.vpW); vp.push_back(dc.camera.vpH);
        n["viewport"]      = vp;
        n["is2D"]          = dc.camera.is2D;
        YAML::Node pan(YAML::NodeType::Sequence);
        pan.push_back(dc.camera.pan2dX); pan.push_back(dc.camera.pan2dY);
        n["pan2d"]         = pan;
        n["zoom2d"]        = dc.camera.zoom2d;
        n["rot2d"]         = dc.camera.rot2dDeg;
        n["worldHeight2d"] = dc.camera.worldHeight2d;
        break;
    }
    case bgi::DdsObjectType::Ucs: {
        const auto &du = static_cast<const bgi::DdsUcs&>(obj);
        n["name"]   = du.name;
        n["origin"] = vec3ToY({du.ucs.originX, du.ucs.originY, du.ucs.originZ});
        n["xAxis"]  = vec3ToY({du.ucs.xAxisX,  du.ucs.xAxisY,  du.ucs.xAxisZ});
        n["yAxis"]  = vec3ToY({du.ucs.yAxisX,  du.ucs.yAxisY,  du.ucs.yAxisZ});
        n["zAxis"]  = vec3ToY({du.ucs.zAxisX,  du.ucs.zAxisY,  du.ucs.zAxisZ});
        break;
    }
    case bgi::DdsObjectType::WorldExtentsObj: {
        const auto &we = static_cast<const bgi::DdsWorldExtentsObj&>(obj);
        n["hasData"] = we.extents.hasData;
        n["min"]     = vec3ToY({we.extents.minX, we.extents.minY, we.extents.minZ});
        n["max"]     = vec3ToY({we.extents.maxX, we.extents.maxY, we.extents.maxZ});
        break;
    }
    case bgi::DdsObjectType::Point: {
        const auto &o = static_cast<const bgi::DdsPoint&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["pos"]        = vec3ToY(o.pos);
        n["color"]      = o.color;
        break;
    }
    case bgi::DdsObjectType::Line: {
        const auto &o = static_cast<const bgi::DdsLine&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["p1"]         = vec3ToY(o.p1);
        n["p2"]         = vec3ToY(o.p2);
        break;
    }
    case bgi::DdsObjectType::Circle: {
        const auto &o = static_cast<const bgi::DdsCircle&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["radius"]     = o.radius;
        break;
    }
    case bgi::DdsObjectType::Arc: {
        const auto &o = static_cast<const bgi::DdsArc&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["radius"]     = o.radius;
        n["startAngle"] = o.startAngle;
        n["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Ellipse: {
        const auto &o = static_cast<const bgi::DdsEllipse&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["rx"]         = o.rx;
        n["ry"]         = o.ry;
        n["startAngle"] = o.startAngle;
        n["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::FillEllipse: {
        const auto &o = static_cast<const bgi::DdsFillEllipse&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["rx"]         = o.rx;
        n["ry"]         = o.ry;
        break;
    }
    case bgi::DdsObjectType::PieSlice: {
        const auto &o = static_cast<const bgi::DdsPieSlice&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["radius"]     = o.radius;
        n["startAngle"] = o.startAngle;
        n["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Sector: {
        const auto &o = static_cast<const bgi::DdsSector&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["centre"]     = vec3ToY(o.centre);
        n["rx"]         = o.rx;
        n["ry"]         = o.ry;
        n["startAngle"] = o.startAngle;
        n["endAngle"]   = o.endAngle;
        break;
    }
    case bgi::DdsObjectType::Rectangle: {
        const auto &o = static_cast<const bgi::DdsRectangle&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["p1"] = vec3ToY(o.p1); n["p2"] = vec3ToY(o.p2);
        break;
    }
    case bgi::DdsObjectType::Bar: {
        const auto &o = static_cast<const bgi::DdsBar&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["p1"] = vec3ToY(o.p1); n["p2"] = vec3ToY(o.p2);
        break;
    }
    case bgi::DdsObjectType::Bar3D: {
        const auto &o = static_cast<const bgi::DdsBar3D&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["p1"]      = vec3ToY(o.p1); n["p2"] = vec3ToY(o.p2);
        n["depth"]   = o.depth;
        n["topFlag"] = o.topFlag;
        break;
    }
    case bgi::DdsObjectType::Polygon: {
        const auto &o = static_cast<const bgi::DdsPolygon&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        YAML::Node pts(YAML::NodeType::Sequence);
        for (const auto &p : o.pts) pts.push_back(vec3ToY(p));
        n["pts"] = pts;
        break;
    }
    case bgi::DdsObjectType::FillPoly: {
        const auto &o = static_cast<const bgi::DdsFillPoly&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        YAML::Node pts(YAML::NodeType::Sequence);
        for (const auto &p : o.pts) pts.push_back(vec3ToY(p));
        n["pts"] = pts;
        break;
    }
    case bgi::DdsObjectType::Text: {
        const auto &o = static_cast<const bgi::DdsText&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["pos"]        = vec3ToY(o.pos);
        n["text"]       = o.text;
        break;
    }
    case bgi::DdsObjectType::Image: {
        const auto &o = static_cast<const bgi::DdsImage&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["style"]      = styleToY(o.style);
        n["pos"]        = vec3ToY(o.pos);
        n["width"]      = o.width;
        n["height"]     = o.height;
        // Omit pixel data in YAML (large binary blob); width/height preserved.
        break;
    }
    // Phase 4/5/6 — solid base fields helper (lambda defined inline)
    case bgi::DdsObjectType::Box: {
        const auto &o = static_cast<const bgi::DdsBox&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["origin"]     = vec3ToY(o.origin);
        n["drawMode"]   = static_cast<int>(o.drawMode);
        n["edgeColor"]  = o.edgeColor;
        n["faceColor"]  = o.faceColor;
        n["slices"]     = o.slices;
        n["stacks"]     = o.stacks;
        n["width"]      = o.width;
        n["depth"]      = o.depth;
        n["height"]     = o.height;
        break;
    }
    case bgi::DdsObjectType::Sphere: {
        const auto &o = static_cast<const bgi::DdsSphere&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["origin"]     = vec3ToY(o.origin);
        n["drawMode"]   = static_cast<int>(o.drawMode);
        n["edgeColor"]  = o.edgeColor;
        n["faceColor"]  = o.faceColor;
        n["slices"]     = o.slices;
        n["stacks"]     = o.stacks;
        n["radius"]     = o.radius;
        break;
    }
    case bgi::DdsObjectType::Cylinder: {
        const auto &o = static_cast<const bgi::DdsCylinder&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["origin"]     = vec3ToY(o.origin);
        n["drawMode"]   = static_cast<int>(o.drawMode);
        n["edgeColor"]  = o.edgeColor;
        n["faceColor"]  = o.faceColor;
        n["slices"]     = o.slices;
        n["stacks"]     = o.stacks;
        n["radius"]     = o.radius;
        n["height"]     = o.height;
        n["caps"]       = o.caps;
        break;
    }
    case bgi::DdsObjectType::Cone: {
        const auto &o = static_cast<const bgi::DdsCone&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["origin"]     = vec3ToY(o.origin);
        n["drawMode"]   = static_cast<int>(o.drawMode);
        n["edgeColor"]  = o.edgeColor;
        n["faceColor"]  = o.faceColor;
        n["slices"]     = o.slices;
        n["stacks"]     = o.stacks;
        n["radius"]     = o.radius;
        n["height"]     = o.height;
        n["cap"]        = o.cap;
        break;
    }
    case bgi::DdsObjectType::Torus: {
        const auto &o = static_cast<const bgi::DdsTorus&>(obj);
        n["coordSpace"]  = coordSpaceStr(o.coordSpace);
        n["ucsName"]     = o.ucsName;
        n["style"]       = styleToY(o.style);
        n["origin"]      = vec3ToY(o.origin);
        n["drawMode"]    = static_cast<int>(o.drawMode);
        n["edgeColor"]   = o.edgeColor;
        n["faceColor"]   = o.faceColor;
        n["slices"]      = o.slices;
        n["stacks"]      = o.stacks;
        n["majorRadius"] = o.majorRadius;
        n["minorRadius"] = o.minorRadius;
        break;
    }
    case bgi::DdsObjectType::HeightMap: {
        const auto &o = static_cast<const bgi::DdsHeightMap&>(obj);
        n["coordSpace"]  = coordSpaceStr(o.coordSpace);
        n["ucsName"]     = o.ucsName;
        n["style"]       = styleToY(o.style);
        n["origin"]      = vec3ToY(o.origin);
        n["drawMode"]    = static_cast<int>(o.drawMode);
        n["edgeColor"]   = o.edgeColor;
        n["faceColor"]   = o.faceColor;
        n["slices"]      = o.slices;
        n["stacks"]      = o.stacks;
        n["rows"]        = o.rows;
        n["cols"]        = o.cols;
        n["cellWidth"]   = o.cellWidth;
        n["cellHeight"]  = o.cellHeight;
        {
            YAML::Node ha(YAML::NodeType::Sequence);
            for (float h : o.heights) ha.push_back(h);
            n["heights"] = ha;
        }
        break;
    }
    case bgi::DdsObjectType::ParamSurface: {
        const auto &o = static_cast<const bgi::DdsParamSurface&>(obj);
        n["coordSpace"] = coordSpaceStr(o.coordSpace);
        n["ucsName"]    = o.ucsName;
        n["style"]      = styleToY(o.style);
        n["origin"]     = vec3ToY(o.origin);
        n["drawMode"]   = static_cast<int>(o.drawMode);
        n["edgeColor"]  = o.edgeColor;
        n["faceColor"]  = o.faceColor;
        n["slices"]     = o.slices;
        n["stacks"]     = o.stacks;
        n["formula"]    = static_cast<int>(o.formula);
        n["param1"]     = o.param1;
        n["param2"]     = o.param2;
        n["uSteps"]     = o.uSteps;
        n["vSteps"]     = o.vSteps;
        break;
    }
    case bgi::DdsObjectType::Extrusion: {
        const auto &o = static_cast<const bgi::DdsExtrusion&>(obj);
        n["coordSpace"]  = coordSpaceStr(o.coordSpace);
        n["ucsName"]     = o.ucsName;
        n["style"]       = styleToY(o.style);
        n["origin"]      = vec3ToY(o.origin);
        n["drawMode"]    = static_cast<int>(o.drawMode);
        n["edgeColor"]   = o.edgeColor;
        n["faceColor"]   = o.faceColor;
        n["slices"]      = o.slices;
        n["stacks"]      = o.stacks;
        n["extrudeDir"]  = vec3ToY(o.extrudeDir);
        n["capStart"]    = o.capStart;
        n["capEnd"]      = o.capEnd;
        {
            YAML::Node pts(YAML::NodeType::Sequence);
            for (const auto &p : o.baseProfile) pts.push_back(vec3ToY(p));
            n["pts"] = pts;
        }
        break;
    }
    default:
        break;
    }
    return n;
}

// YAML object → DdsObject (mirrors objectFromJ, reading from YAML::Node)
std::shared_ptr<bgi::DdsObject> objectFromY(const YAML::Node &n)
{
    const std::string type = n["type"] ? n["type"].as<std::string>() : "";
    const std::string id   = n["id"]   ? n["id"].as<std::string>()   : "";

    auto baseLoad = [&](bgi::DdsObject &o) {
        o.id      = id;
        o.label   = n["label"]   ? n["label"].as<std::string>()   : "";
        o.visible = n["visible"] ? n["visible"].as<bool>(true)     : true;
    };

    if (type == "Camera") {
        auto dc = std::make_shared<bgi::DdsCamera>();
        baseLoad(*dc);
        dc->name = n["name"] ? n["name"].as<std::string>() : "";
        dc->camera.projection = projFromStr(n["projection"] ? n["projection"].as<std::string>() : "Orthographic");
        if (n["eye"])    { auto v = vec3FromY(n["eye"]);    dc->camera.eyeX=v.x; dc->camera.eyeY=v.y; dc->camera.eyeZ=v.z; }
        if (n["target"]) { auto v = vec3FromY(n["target"]); dc->camera.targetX=v.x; dc->camera.targetY=v.y; dc->camera.targetZ=v.z; }
        if (n["up"])     { auto v = vec3FromY(n["up"]);     dc->camera.upX=v.x; dc->camera.upY=v.y; dc->camera.upZ=v.z; }
        dc->camera.fovYDeg  = n["fovY"] ? n["fovY"].as<float>(45.f) : 45.f;
        dc->camera.nearPlane= n["near"] ? n["near"].as<float>(-1.f) : -1.f;
        dc->camera.farPlane = n["far"]  ? n["far"].as<float>(1.f)   :  1.f;
        if (n["ortho"] && n["ortho"].size() >= 4) {
            dc->camera.orthoLeft   = n["ortho"][0].as<float>(0.f);
            dc->camera.orthoRight  = n["ortho"][1].as<float>(0.f);
            dc->camera.orthoBottom = n["ortho"][2].as<float>(0.f);
            dc->camera.orthoTop    = n["ortho"][3].as<float>(0.f);
        }
        if (n["viewport"] && n["viewport"].size() >= 4) {
            dc->camera.vpX = n["viewport"][0].as<int>(0);
            dc->camera.vpY = n["viewport"][1].as<int>(0);
            dc->camera.vpW = n["viewport"][2].as<int>(0);
            dc->camera.vpH = n["viewport"][3].as<int>(0);
        }
        dc->camera.is2D          = n["is2D"] ? n["is2D"].as<bool>(false) : false;
        if (n["pan2d"] && n["pan2d"].size() >= 2) {
            dc->camera.pan2dX = n["pan2d"][0].as<float>(0.f);
            dc->camera.pan2dY = n["pan2d"][1].as<float>(0.f);
        }
        dc->camera.zoom2d        = n["zoom2d"]        ? n["zoom2d"].as<float>(1.f)        : 1.f;
        dc->camera.rot2dDeg      = n["rot2d"]         ? n["rot2d"].as<float>(0.f)         : 0.f;
        dc->camera.worldHeight2d = n["worldHeight2d"] ? n["worldHeight2d"].as<float>(2.f) : 2.f;
        return std::static_pointer_cast<bgi::DdsObject>(dc);
    }
    if (type == "Ucs") {
        auto du = std::make_shared<bgi::DdsUcs>();
        baseLoad(*du);
        du->name = n["name"] ? n["name"].as<std::string>() : "";
        if (n["origin"]) { auto v = vec3FromY(n["origin"]); du->ucs.originX=v.x; du->ucs.originY=v.y; du->ucs.originZ=v.z; }
        if (n["xAxis"])  { auto v = vec3FromY(n["xAxis"]);  du->ucs.xAxisX=v.x;  du->ucs.xAxisY=v.y;  du->ucs.xAxisZ=v.z;  }
        if (n["yAxis"])  { auto v = vec3FromY(n["yAxis"]);  du->ucs.yAxisX=v.x;  du->ucs.yAxisY=v.y;  du->ucs.yAxisZ=v.z;  }
        if (n["zAxis"])  { auto v = vec3FromY(n["zAxis"]);  du->ucs.zAxisX=v.x;  du->ucs.zAxisY=v.y;  du->ucs.zAxisZ=v.z;  }
        return std::static_pointer_cast<bgi::DdsObject>(du);
    }
    if (type == "WorldExtents") {
        auto we = std::make_shared<bgi::DdsWorldExtentsObj>();
        baseLoad(*we);
        we->extents.hasData = n["hasData"] ? n["hasData"].as<bool>(false) : false;
        if (n["min"]) { auto v = vec3FromY(n["min"]); we->extents.minX=v.x; we->extents.minY=v.y; we->extents.minZ=v.z; }
        if (n["max"]) { auto v = vec3FromY(n["max"]); we->extents.maxX=v.x; we->extents.maxY=v.y; we->extents.maxZ=v.z; }
        return std::static_pointer_cast<bgi::DdsObject>(we);
    }

    // Drawing objects — build a minimal JSON fragment and reuse objectFromJ
    // (avoids duplicating all the drawing deserialization logic).
    // This is safe because yaml-cpp and nlohmann/json both work in-memory.
    // Performance is adequate for a one-time load operation.
    try {
        // Emit YAML node to string, then parse as JSON.
        // Unfortunately YAML→JSON direct is not trivial; we rebuild a json object.
        // For drawing objects we just re-implement the few needed fields directly.
        auto getStr = [&](const char *key, const std::string &def = {}) -> std::string {
            return n[key] ? n[key].as<std::string>(def) : def;
        };
        auto getFloat = [&](const char *key, float def = 0.f) -> float {
            return n[key] ? n[key].as<float>(def) : def;
        };
        auto getBool = [&](const char *key, bool def = true) -> bool {
            return n[key] ? n[key].as<bool>(def) : def;
        };
        auto getVec3 = [&](const char *key) -> glm::vec3 {
            return n[key] ? vec3FromY(n[key]) : glm::vec3{0,0,0};
        };
        auto getStyle = [&]() -> bgi::DdsStyle {
            return n["style"] ? styleFromY(n["style"]) : bgi::DdsStyle{};
        };
        auto getCS = [&]() -> bgi::CoordSpace {
            return coordSpaceFromStr(getStr("coordSpace", "BgiPixel"));
        };
        auto getUcsName = [&]() -> std::string { return getStr("ucsName"); };

        if (type == "Point") {
            auto o = std::make_shared<bgi::DdsPoint>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->pos = getVec3("pos");  o->color = n["color"] ? n["color"].as<int>(0) : 0;
            return o;
        }
        if (type == "Line") {
            auto o = std::make_shared<bgi::DdsLine>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->p1 = getVec3("p1"); o->p2 = getVec3("p2");
            return o;
        }
        if (type == "Circle") {
            auto o = std::make_shared<bgi::DdsCircle>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->radius = getFloat("radius");
            return o;
        }
        if (type == "Arc") {
            auto o = std::make_shared<bgi::DdsArc>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->radius = getFloat("radius");
            o->startAngle = getFloat("startAngle"); o->endAngle = getFloat("endAngle");
            return o;
        }
        if (type == "Ellipse") {
            auto o = std::make_shared<bgi::DdsEllipse>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->rx = getFloat("rx"); o->ry = getFloat("ry");
            o->startAngle = getFloat("startAngle"); o->endAngle = getFloat("endAngle");
            return o;
        }
        if (type == "FillEllipse") {
            auto o = std::make_shared<bgi::DdsFillEllipse>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->rx = getFloat("rx"); o->ry = getFloat("ry");
            return o;
        }
        if (type == "PieSlice") {
            auto o = std::make_shared<bgi::DdsPieSlice>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->radius = getFloat("radius");
            o->startAngle = getFloat("startAngle"); o->endAngle = getFloat("endAngle");
            return o;
        }
        if (type == "Sector") {
            auto o = std::make_shared<bgi::DdsSector>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->centre = getVec3("centre"); o->rx = getFloat("rx"); o->ry = getFloat("ry");
            o->startAngle = getFloat("startAngle"); o->endAngle = getFloat("endAngle");
            return o;
        }
        if (type == "Rectangle") {
            auto o = std::make_shared<bgi::DdsRectangle>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->p1 = getVec3("p1"); o->p2 = getVec3("p2");
            return o;
        }
        if (type == "Bar") {
            auto o = std::make_shared<bgi::DdsBar>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->p1 = getVec3("p1"); o->p2 = getVec3("p2");
            return o;
        }
        if (type == "Bar3D") {
            auto o = std::make_shared<bgi::DdsBar3D>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->p1 = getVec3("p1"); o->p2 = getVec3("p2");
            o->depth = getFloat("depth"); o->topFlag = getBool("topFlag");
            return o;
        }
        if (type == "Polygon") {
            auto o = std::make_shared<bgi::DdsPolygon>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            if (n["pts"]) for (const auto &p : n["pts"]) o->pts.push_back(vec3FromY(p));
            return o;
        }
        if (type == "FillPoly") {
            auto o = std::make_shared<bgi::DdsFillPoly>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            if (n["pts"]) for (const auto &p : n["pts"]) o->pts.push_back(vec3FromY(p));
            return o;
        }
        if (type == "Text") {
            auto o = std::make_shared<bgi::DdsText>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->ucsName = getUcsName(); o->style = getStyle();
            o->pos = getVec3("pos"); o->text = getStr("text");
            return o;
        }
        if (type == "Image") {
            auto o = std::make_shared<bgi::DdsImage>();
            baseLoad(*o);
            o->coordSpace = getCS(); o->style = getStyle();
            o->pos = getVec3("pos");
            o->width  = n["width"]  ? n["width"].as<int>(0)  : 0;
            o->height = n["height"] ? n["height"].as<int>(0) : 0;
            // Pixel data not restored from YAML (was not serialized).
            return o;
        }

        // Phase 4/5/6 — solid base field loader (lambda)
        auto loadSolidBase = [&](bgi::DdsSolid3D &s) {
            s.coordSpace = getCS(); s.ucsName = getUcsName(); s.style = getStyle();
            if (n["origin"]) { auto v = vec3FromY(n["origin"]); s.origin = {v.x, v.y, v.z}; }
            s.drawMode  = static_cast<bgi::SolidDrawMode>(n["drawMode"]  ? n["drawMode"].as<int>(0)  : 0);
            s.edgeColor = n["edgeColor"] ? n["edgeColor"].as<int>(15) : 15;
            s.faceColor = n["faceColor"] ? n["faceColor"].as<int>(7)  :  7;
            s.slices    = n["slices"]    ? n["slices"].as<int>(16)    : 16;
            s.stacks    = n["stacks"]    ? n["stacks"].as<int>(8)     :  8;
        };

        if (type == "Box") {
            auto o = std::make_shared<bgi::DdsBox>();
            baseLoad(*o); loadSolidBase(*o);
            o->width  = getFloat("width",  1.f);
            o->depth  = getFloat("depth",  1.f);
            o->height = getFloat("height", 1.f);
            return o;
        }
        if (type == "Sphere") {
            auto o = std::make_shared<bgi::DdsSphere>();
            baseLoad(*o); loadSolidBase(*o);
            o->radius = getFloat("radius", 1.f);
            return o;
        }
        if (type == "Cylinder") {
            auto o = std::make_shared<bgi::DdsCylinder>();
            baseLoad(*o); loadSolidBase(*o);
            o->radius = getFloat("radius", 1.f);
            o->height = getFloat("height", 1.f);
            o->caps   = n["caps"] ? n["caps"].as<int>(1) : 1;
            return o;
        }
        if (type == "Cone") {
            auto o = std::make_shared<bgi::DdsCone>();
            baseLoad(*o); loadSolidBase(*o);
            o->radius = getFloat("radius", 1.f);
            o->height = getFloat("height", 1.f);
            o->cap    = n["cap"] ? n["cap"].as<int>(1) : 1;
            return o;
        }
        if (type == "Torus") {
            auto o = std::make_shared<bgi::DdsTorus>();
            baseLoad(*o); loadSolidBase(*o);
            o->majorRadius = getFloat("majorRadius", 2.f);
            o->minorRadius = getFloat("minorRadius", 0.5f);
            return o;
        }
        if (type == "HeightMap") {
            auto o = std::make_shared<bgi::DdsHeightMap>();
            baseLoad(*o); loadSolidBase(*o);
            o->rows       = n["rows"]  ? n["rows"].as<int>(0)    : 0;
            o->cols       = n["cols"]  ? n["cols"].as<int>(0)    : 0;
            o->cellWidth  = getFloat("cellWidth",  1.f);
            o->cellHeight = getFloat("cellHeight", 1.f);
            if (n["heights"] && n["heights"].IsSequence())
                for (const auto &h : n["heights"])
                    o->heights.push_back(h.as<float>(0.f));
            return o;
        }
        if (type == "ParamSurface") {
            auto o = std::make_shared<bgi::DdsParamSurface>();
            baseLoad(*o); loadSolidBase(*o);
            o->formula = static_cast<bgi::ParamSurfaceFormula>(n["formula"] ? n["formula"].as<int>(0) : 0);
            o->param1  = getFloat("param1", 1.f);
            o->param2  = getFloat("param2", 0.5f);
            o->uSteps  = n["uSteps"] ? n["uSteps"].as<int>(20) : 20;
            o->vSteps  = n["vSteps"] ? n["vSteps"].as<int>(20) : 20;
            return o;
        }
        if (type == "Extrusion") {
            auto o = std::make_shared<bgi::DdsExtrusion>();
            baseLoad(*o); loadSolidBase(*o);
            if (n["extrudeDir"]) { auto v = vec3FromY(n["extrudeDir"]); o->extrudeDir = {v.x, v.y, v.z}; }
            o->capStart = n["capStart"] ? n["capStart"].as<int>(1) : 1;
            o->capEnd   = n["capEnd"]   ? n["capEnd"].as<int>(1)   : 1;
            if (n["pts"] && n["pts"].IsSequence())
                for (const auto &p : n["pts"]) o->baseProfile.push_back(vec3FromY(p));
            return o;
        }
    }
    catch (...) {}
    return nullptr;
}

void sceneFromYaml(const YAML::Node &root)
{
    bgi::gState.dds->clearAll();
    bgi::gState.cameras.clear();
    bgi::gState.ucsSystems.clear();
    bgi::gState.worldExtents = bgi::WorldExtents{};

    if (root["worldExtents"]) {
        const auto &we = root["worldExtents"];
        bgi::gState.worldExtents.hasData = we["hasData"] ? we["hasData"].as<bool>(false) : false;
        if (we["min"]) { auto v = vec3FromY(we["min"]); bgi::gState.worldExtents.minX=v.x; bgi::gState.worldExtents.minY=v.y; bgi::gState.worldExtents.minZ=v.z; }
        if (we["max"]) { auto v = vec3FromY(we["max"]); bgi::gState.worldExtents.maxX=v.x; bgi::gState.worldExtents.maxY=v.y; bgi::gState.worldExtents.maxZ=v.z; }
    }

    if (root["objects"] && root["objects"].IsSequence()) {
        for (const auto &yo : root["objects"]) {
            auto obj = objectFromY(yo);
            if (!obj) continue;
            if (obj->type == bgi::DdsObjectType::Camera) {
                auto dc = std::static_pointer_cast<bgi::DdsCamera>(obj);
                bgi::gState.dds->appendWithId(dc);
                bgi::gState.cameras[dc->name] = dc;
            }
            else if (obj->type == bgi::DdsObjectType::Ucs) {
                auto du = std::static_pointer_cast<bgi::DdsUcs>(obj);
                bgi::gState.dds->appendWithId(du);
                bgi::gState.ucsSystems[du->name] = du;
            }
            else {
                bgi::gState.dds->appendWithId(obj);
            }
        }
    }

    bgi::gState.activeCamera = root["activeCamera"] ? root["activeCamera"].as<std::string>("default") : "default";
    if (!bgi::gState.cameras.count(bgi::gState.activeCamera))
        bgi::gState.activeCamera = "default";

    bgi::gState.activeUcs = root["activeUcs"] ? root["activeUcs"].as<std::string>("world") : "world";
    if (!bgi::gState.ucsSystems.count(bgi::gState.activeUcs))
        bgi::gState.activeUcs = "world";

    // Ensure mandatory defaults.
    if (!bgi::gState.cameras.count("default")) {
        auto dc = std::make_shared<bgi::DdsCamera>();
        dc->name = "default";
        dc->camera.projection  = bgi::CameraProjection::Orthographic;
        dc->camera.eyeX=0.f; dc->camera.eyeY=0.f; dc->camera.eyeZ=1.f;
        dc->camera.upX=0.f;  dc->camera.upY=1.f;  dc->camera.upZ=0.f;
        dc->camera.orthoLeft=0.f; dc->camera.orthoRight=static_cast<float>(bgi::gState.width);
        dc->camera.orthoBottom=static_cast<float>(bgi::gState.height); dc->camera.orthoTop=0.f;
        dc->camera.nearPlane=-1.f; dc->camera.farPlane=1.f;
        bgi::gState.dds->append(dc);
        bgi::gState.cameras["default"] = dc;
    }
    if (!bgi::gState.ucsSystems.count("world")) {
        auto du = std::make_shared<bgi::DdsUcs>();
        du->name = "world"; du->ucs = bgi::CoordSystem{};
        bgi::gState.dds->append(du);
        bgi::gState.ucsSystems["world"] = du;
    }
}

} // anonymous namespace

// =============================================================================
// Public API implementation
// =============================================================================

BGI_API const char *BGI_CALL wxbgi_dds_to_json(void)
{
    static thread_local std::string result;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    try {
        result = sceneToJson().dump(2);
    } catch (...) {
        result = "{}";
    }
    return result.c_str();
}

BGI_API const char *BGI_CALL wxbgi_dds_to_yaml(void)
{
    static thread_local std::string result;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    try {
        YAML::Node root;
        root["version"]      = kDdsVersion;
        root["activeCamera"] = bgi::gState.activeCamera;
        root["activeUcs"]    = bgi::gState.activeUcs;

        YAML::Node we;
        we["hasData"] = bgi::gState.worldExtents.hasData;
        we["min"]     = vec3ToY({bgi::gState.worldExtents.minX, bgi::gState.worldExtents.minY, bgi::gState.worldExtents.minZ});
        we["max"]     = vec3ToY({bgi::gState.worldExtents.maxX, bgi::gState.worldExtents.maxY, bgi::gState.worldExtents.maxZ});
        root["worldExtents"] = we;

        YAML::Node objs(YAML::NodeType::Sequence);
        bgi::gState.dds->forEach([&](const bgi::DdsObject &obj) {
            objs.push_back(objectToY(obj));
        });
        root["objects"] = objs;

        std::ostringstream oss;
        oss << root;
        result = oss.str();
    } catch (...) {
        result = "";
    }
    return result.c_str();
}

BGI_API int BGI_CALL wxbgi_dds_from_json(const char *jsonString)
{
    if (jsonString == nullptr || jsonString[0] == '\0')
        return -1;
    try {
        auto root = json::parse(jsonString);
        std::lock_guard<std::mutex> lock(bgi::gMutex);
        sceneFromJson(root);
        return 0;
    } catch (...) {
        return -1;
    }
}

BGI_API int BGI_CALL wxbgi_dds_from_yaml(const char *yamlString)
{
    if (yamlString == nullptr || yamlString[0] == '\0')
        return -1;
    try {
        auto root = YAML::Load(yamlString);
        std::lock_guard<std::mutex> lock(bgi::gMutex);
        sceneFromYaml(root);
        return 0;
    } catch (...) {
        return -1;
    }
}

BGI_API int BGI_CALL wxbgi_dds_save_json(const char *filePath)
{
    if (filePath == nullptr || filePath[0] == '\0')
        return -1;
    try {
        std::string s;
        {
            std::lock_guard<std::mutex> lock(bgi::gMutex);
            s = sceneToJson().dump(2);
        }
        std::ofstream f(filePath);
        if (!f) return -1;
        f << s;
        return f.good() ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

BGI_API int BGI_CALL wxbgi_dds_load_json(const char *filePath)
{
    if (filePath == nullptr || filePath[0] == '\0')
        return -1;
    try {
        std::ifstream f(filePath);
        if (!f) return -1;
        auto root = json::parse(f);
        std::lock_guard<std::mutex> lock(bgi::gMutex);
        sceneFromJson(root);
        return 0;
    } catch (...) {
        return -1;
    }
}

BGI_API int BGI_CALL wxbgi_dds_save_yaml(const char *filePath)
{
    if (filePath == nullptr || filePath[0] == '\0')
        return -1;
    try {
        std::string s;
        {
            std::lock_guard<std::mutex> lock(bgi::gMutex);
            // Reuse the to_yaml buffer logic inline.
            YAML::Node root;
            root["version"]      = kDdsVersion;
            root["activeCamera"] = bgi::gState.activeCamera;
            root["activeUcs"]    = bgi::gState.activeUcs;
            YAML::Node we;
            we["hasData"] = bgi::gState.worldExtents.hasData;
            we["min"]     = vec3ToY({bgi::gState.worldExtents.minX, bgi::gState.worldExtents.minY, bgi::gState.worldExtents.minZ});
            we["max"]     = vec3ToY({bgi::gState.worldExtents.maxX, bgi::gState.worldExtents.maxY, bgi::gState.worldExtents.maxZ});
            root["worldExtents"] = we;
            YAML::Node objs(YAML::NodeType::Sequence);
            bgi::gState.dds->forEach([&](const bgi::DdsObject &obj) {
                objs.push_back(objectToY(obj));
            });
            root["objects"] = objs;
            std::ostringstream oss;
            oss << root;
            s = oss.str();
        }
        std::ofstream f(filePath);
        if (!f) return -1;
        f << s;
        return f.good() ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

BGI_API int BGI_CALL wxbgi_dds_load_yaml(const char *filePath)
{
    if (filePath == nullptr || filePath[0] == '\0')
        return -1;
    try {
        auto root = YAML::LoadFile(filePath);
        std::lock_guard<std::mutex> lock(bgi::gMutex);
        sceneFromYaml(root);
        return 0;
    } catch (...) {
        return -1;
    }
}
