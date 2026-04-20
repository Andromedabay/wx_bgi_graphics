#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"
#include "wx_bgi_solid.h"

#include <olb.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace bgi;
using namespace olb;
using namespace olb::descriptors;

namespace
{

using T = double;
using DESCRIPTOR = D3Q19<>;

constexpr int kMatFluid    = 1;
constexpr int kMatWall     = 2;
constexpr int kMatInflow   = 3;
constexpr int kMatOutflow  = 4;
constexpr int kMatSieve    = 5;
constexpr int kMatSideVent = 6;

constexpr T kPipeLength      = 1.80;
constexpr T kPipeWidth       = 0.36;
constexpr T kPipeHeight      = 0.24;
constexpr T kWallThickness   = 0.03;
constexpr T kSieveX          = 0.15;
constexpr T kSieveThickness  = 0.03;
constexpr T kSieveHoleDiameter = 0.018;
constexpr int kSieveHoleCols = 10;
constexpr int kSieveHoleRows = 6;
constexpr T kVentCenterX     = 0.95;
constexpr T kVentWidthX      = 0.14;
constexpr T kVentHeightZ     = 0.08;
constexpr T kVentCenterZ     = kPipeHeight * 0.5;
constexpr T kCharPhysLength  = 0.10;
constexpr T kCharPhysVelocity = 0.06;
constexpr T kPhysViscosity   = 0.0006;
constexpr T kPhysDensity     = 1.0;
constexpr T kCharLatticeU    = 0.02;
constexpr int kResolution    = 5;

constexpr int kWindowMargin   = 12;
constexpr int kPanelGap       = 18;
constexpr int kPreviewTop     = 16;
constexpr int kPreviewHeight  = 220;
constexpr int kFieldCellPx    = 12;
constexpr int kFlowPanelHeight = 220;
constexpr int kHudPanelWidth  = 320;
constexpr int kLegendWidthPx = 18;
constexpr std::size_t kRampSteps = 180;
constexpr T kInflowPeakFactor = 1.5;
constexpr int kModeWireframe = WXBGI_SOLID_WIREFRAME;
constexpr int kModeFlat      = WXBGI_SOLID_FLAT;
constexpr int kModeSmooth    = WXBGI_SOLID_SMOOTH;
constexpr float kFlowCamAzimuthDefaultDeg = -118.f;
constexpr float kFlowCamElevationDefaultDeg = 26.f;
constexpr float kFlowCamOrbitRadius = 1.55f;
constexpr float kFlowCamStepDeg = 6.f;

struct SieveLayout
{
    std::vector<float> holeYs;
    std::vector<float> holeZs;
};

struct DemoLayout
{
    int windowW = 0;
    int windowH = 0;
    int previewW = 0;
    int fieldLeft = 0;
    int fieldTop = 0;
    int legendLeft = 0;
    int flowLeft = 0;
    int flowTop = 0;
    int flowW = 0;
    int hudLeft = 0;
    int hudTop = 0;
    int hudW = 0;
    int hudH = 0;
};

struct InputLatch
{
    bool shade1 = false;
    bool shade2 = false;
    bool shade3 = false;
    bool orbitLeft = false;
    bool orbitRight = false;
    bool orbitUp = false;
    bool orbitDown = false;
    bool slicePlus = false;
    bool sliceMinus = false;
};

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [wxbgi_openlb_pipe_3d_demo]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

std::string lastId()
{
    return wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
}

std::string createBox(float cx, float cy, float cz, float sx, float sy, float sz)
{
    wxbgi_solid_box(cx, cy, cz, sx, sy, sz);
    const std::string id = lastId();
    require(!id.empty(), "Failed to create DDS box");
    return id;
}

std::string createDifference(const std::vector<std::string> &ids, const char *label)
{
    std::vector<const char *> ptrs;
    ptrs.reserve(ids.size());
    for (const std::string &id : ids)
        ptrs.push_back(id.c_str());

    const std::string diffId = wxbgi_dds_difference(static_cast<int>(ptrs.size()), ptrs.data());
    require(!diffId.empty(), "DDS difference returned empty id");
    if (label != nullptr && *label != '\0')
        wxbgi_dds_set_label(diffId.c_str(), label);
    return diffId;
}

std::string createUnion(const std::vector<std::string> &ids, const char *label)
{
    std::vector<const char *> ptrs;
    ptrs.reserve(ids.size());
    for (const std::string &id : ids)
        ptrs.push_back(id.c_str());

    const std::string unionId = wxbgi_dds_union(static_cast<int>(ptrs.size()), ptrs.data());
    require(!unionId.empty(), "DDS union returned empty id");
    if (label != nullptr && *label != '\0')
        wxbgi_dds_set_label(unionId.c_str(), label);
    return unionId;
}

SieveLayout buildDdsPipeScene(const UnitConverter<T, DESCRIPTOR> &converter)
{
    wxbgi_dds_scene_set_active("default");
    wxbgi_dds_clear();
    SieveLayout layout;

    const float outerCx = static_cast<float>(kPipeLength * 0.5);
    const float outerCy = static_cast<float>(kPipeWidth * 0.5);
    const float outerCz = static_cast<float>(kPipeHeight * 0.5);

    const std::string shellOuter = createBox(outerCx, outerCy, outerCz,
                                             static_cast<float>(kPipeLength),
                                             static_cast<float>(kPipeWidth + 2. * kWallThickness),
                                             static_cast<float>(kPipeHeight + 2. * kWallThickness));
    const std::string shellInner = createBox(outerCx, outerCy, outerCz,
                                             static_cast<float>(kPipeLength),
                                             static_cast<float>(kPipeWidth),
                                             static_cast<float>(kPipeHeight));
    const std::string shellBase = createDifference({shellOuter, shellInner}, "pipe-shell-base");

    const std::string leftHole = createBox(static_cast<float>(kVentCenterX),
                                           0.f,
                                           static_cast<float>(kVentCenterZ),
                                           static_cast<float>(kVentWidthX),
                                           static_cast<float>(3. * kWallThickness),
                                           static_cast<float>(kVentHeightZ));
    const std::string shellId = createDifference({shellBase, leftHole}, "pipe-shell");
    require(wxbgi_dds_set_external_attr(shellId.c_str(), "openlb.enabled", "0") == 1,
            "Failed to disable pipe shell for OpenLB export");

    const T requestedHole = kSieveHoleDiameter;
    const T holeY = std::min(requestedHole, kPipeWidth / static_cast<T>(kSieveHoleCols + 1));
    const T holeZ = std::min(requestedHole, kPipeHeight / static_cast<T>(kSieveHoleRows + 1));
    const T barY = (kPipeWidth - static_cast<T>(kSieveHoleCols) * holeY) /
                   static_cast<T>(kSieveHoleCols + 1);
    const T barZ = (kPipeHeight - static_cast<T>(kSieveHoleRows) * holeZ) /
                   static_cast<T>(kSieveHoleRows + 1);
    require(barY > 0.0 && barZ > 0.0, "Sieve hole diameter is too large for the selected grid");

    std::vector<std::string> sieveBars;
    sieveBars.reserve(static_cast<std::size_t>(kSieveHoleCols + kSieveHoleRows + 2));

    T yCursor = barY;
    for (int i = 0; i < kSieveHoleCols; ++i)
    {
        layout.holeYs.push_back(static_cast<float>(yCursor + holeY * 0.5));
        yCursor += holeY + barY;
    }

    T zCursor = barZ;
    for (int i = 0; i < kSieveHoleRows; ++i)
    {
        layout.holeZs.push_back(static_cast<float>(zCursor + holeZ * 0.5));
        zCursor += holeZ + barZ;
    }

    T barCenterY = barY * 0.5;
    for (int i = 0; i <= kSieveHoleCols; ++i)
    {
        sieveBars.push_back(createBox(static_cast<float>(kSieveX),
                                      static_cast<float>(barCenterY),
                                      static_cast<float>(kPipeHeight * 0.5),
                                      static_cast<float>(kSieveThickness),
                                      static_cast<float>(barY),
                                      static_cast<float>(kPipeHeight)));
        barCenterY += holeY + barY;
    }

    T barCenterZ = barZ * 0.5;
    for (int i = 0; i <= kSieveHoleRows; ++i)
    {
        sieveBars.push_back(createBox(static_cast<float>(kSieveX),
                                      static_cast<float>(kPipeWidth * 0.5),
                                      static_cast<float>(barCenterZ),
                                      static_cast<float>(kSieveThickness),
                                      static_cast<float>(kPipeWidth),
                                      static_cast<float>(barZ)));
        barCenterZ += holeZ + barZ;
    }

    const std::string sieveId = createUnion(sieveBars, "sieve-plate");
    require(wxbgi_dds_set_external_attr(sieveId.c_str(), "openlb.role", "solid") == 1,
            "Failed to tag sieve role");
    require(wxbgi_dds_set_external_attr(sieveId.c_str(), "openlb.material", "5") == 1,
            "Failed to tag sieve material");
    require(wxbgi_dds_set_external_attr(sieveId.c_str(), "openlb.boundary", "sieve") == 1,
            "Failed to tag sieve boundary");
    return layout;
}

const char *renderModeLabel(int mode)
{
    switch (mode)
    {
    case kModeWireframe: return "wireframe";
    case kModeFlat:      return "flat";
    default:             return "smooth";
    }
}

void applyRenderMode(int mode)
{
    wxbgi_solid_set_draw_mode(mode);
    wxbgi_dds_set_solid_draw_mode(mode);
}

void updatePreviewCameraOrbit(std::size_t frame, int viewportWidth)
{
    const float angle = static_cast<float>(frame) * 0.045f;
    const float cx = static_cast<float>(kPipeLength * 0.5);
    const float cy = static_cast<float>(kPipeWidth * 0.5);
    const float cz = static_cast<float>(kPipeHeight * 0.38);
    const float radiusX = 1.95f;
    const float radiusY = 1.35f;

    wxbgi_cam_set_eye("pipe3d_preview",
                      cx + std::cos(angle) * radiusX,
                      cy + std::sin(angle) * radiusY,
                      0.56f);
    wxbgi_cam_set_target("pipe3d_preview",
                          cx,
                          cy,
                          cz);
    wxbgi_cam_set_up("pipe3d_preview", 0.f, 0.f, 1.f);
    wxbgi_cam_set_perspective("pipe3d_preview", 24.f, 0.05f, 12.f);
    wxbgi_cam_set_screen_viewport("pipe3d_preview",
                                  kWindowMargin,
                                  kPreviewTop,
                                  viewportWidth,
                                  kPreviewHeight);
}

void updateFlowCameraOrbit(float azimuthDeg, float elevationDeg)
{
    const float targetX = static_cast<float>(kPipeLength * 0.55);
    const float targetY = static_cast<float>(kPipeWidth * 0.5);
    const float targetZ = static_cast<float>(kPipeHeight * 0.72);
    const float az = azimuthDeg * 3.14159265358979323846f / 180.f;
    const float el = elevationDeg * 3.14159265358979323846f / 180.f;
    const float planar = kFlowCamOrbitRadius * std::cos(el);
    const float eyeX = targetX + planar * std::cos(az);
    const float eyeY = targetY + planar * std::sin(az);
    const float eyeZ = targetZ + kFlowCamOrbitRadius * std::sin(el);

    wxbgi_cam_set_eye("pipe3d_flow3d", eyeX, eyeY, eyeZ);
    wxbgi_cam_set_target("pipe3d_flow3d", targetX, targetY, targetZ);
    wxbgi_cam_set_up("pipe3d_flow3d", 0.f, 0.f, 1.f);
}

bool consumeEdge(bool current, bool &latched)
{
    const bool pressed = current && !latched;
    latched = current;
    return pressed;
}

bool handleRealtimeInput(InputLatch &inputLatch,
                         int &solidMode,
                         float &flowCamAzimuthDeg,
                         float &flowCamElevationDeg,
                         int &sliceZ,
                         int sliceMin,
                         int sliceMax)
{
    if (wxbgi_is_key_down(WXBGI_KEY_ESCAPE) == 1)
        return true;

    if (consumeEdge(wxbgi_is_key_down('1') == 1, inputLatch.shade1))
    {
        solidMode = kModeWireframe;
        applyRenderMode(solidMode);
    }
    if (consumeEdge(wxbgi_is_key_down('2') == 1, inputLatch.shade2))
    {
        solidMode = kModeFlat;
        applyRenderMode(solidMode);
    }
    if (consumeEdge(wxbgi_is_key_down('3') == 1, inputLatch.shade3))
    {
        solidMode = kModeSmooth;
        applyRenderMode(solidMode);
    }
    if (consumeEdge(wxbgi_is_key_down('J') == 1, inputLatch.orbitLeft))
    {
        flowCamAzimuthDeg -= kFlowCamStepDeg;
        updateFlowCameraOrbit(flowCamAzimuthDeg, flowCamElevationDeg);
    }
    if (consumeEdge(wxbgi_is_key_down('L') == 1, inputLatch.orbitRight))
    {
        flowCamAzimuthDeg += kFlowCamStepDeg;
        updateFlowCameraOrbit(flowCamAzimuthDeg, flowCamElevationDeg);
    }
    if (consumeEdge(wxbgi_is_key_down('I') == 1, inputLatch.orbitUp))
    {
        flowCamElevationDeg = std::min(80.f, flowCamElevationDeg + kFlowCamStepDeg);
        updateFlowCameraOrbit(flowCamAzimuthDeg, flowCamElevationDeg);
    }
    if (consumeEdge(wxbgi_is_key_down('K') == 1, inputLatch.orbitDown))
    {
        flowCamElevationDeg = std::max(-10.f, flowCamElevationDeg - kFlowCamStepDeg);
        updateFlowCameraOrbit(flowCamAzimuthDeg, flowCamElevationDeg);
    }
    if (consumeEdge((wxbgi_is_key_down(WXBGI_KEY_EQUAL) == 1) || (wxbgi_is_key_down('+') == 1),
                    inputLatch.slicePlus))
    {
        sliceZ = std::min(sliceMax, sliceZ + 1);
    }
    if (consumeEdge((wxbgi_is_key_down(WXBGI_KEY_MINUS) == 1) || (wxbgi_is_key_down('_') == 1),
                    inputLatch.sliceMinus))
    {
        sliceZ = std::max(sliceMin, sliceZ - 1);
    }

    return false;
}

DemoLayout computeLayout(int sliceCols, int sliceRows)
{
    DemoLayout layout;
    layout.windowW = std::max(1, getmaxx() + 1);
    layout.windowH = std::max(1, getmaxy() + 1);

    const int hudLeft = std::max(kWindowMargin * 2 + 480,
                                 layout.windowW - kWindowMargin - kHudPanelWidth);
    const int contentW = std::max(320, hudLeft - kPanelGap - kWindowMargin);
    const int row2Top = kPreviewTop + kPreviewHeight + kPanelGap;
    const int row2Height = sliceRows * kFieldCellPx;
    const int row3Top = row2Top + row2Height + kPanelGap + 28;

    layout.previewW = contentW;
    layout.fieldLeft = kWindowMargin;
    layout.fieldTop = row2Top;
    layout.legendLeft = layout.fieldLeft + sliceCols * kFieldCellPx + 12;
    layout.flowLeft = kWindowMargin;
    layout.flowTop = row3Top;
    layout.flowW = contentW;
    layout.hudLeft = hudLeft;
    layout.hudTop = kPreviewTop;
    layout.hudW = layout.windowW - layout.hudLeft - kWindowMargin;
    layout.hudH = layout.windowH - layout.hudTop - kWindowMargin;
    return layout;
}

void prepareGeometry(const UnitConverter<T, DESCRIPTOR> &converter,
                     SuperGeometry<T, 3> &geometry,
                     WxbgiOpenLbMaterializeStats &materializeStats)
{
    OstreamManager clout(std::cout, "prepareGeometry3D");
    clout << "Preparing 3D duct geometry..." << std::endl;

    geometry.rename(0, kMatWall);
    geometry.rename(kMatWall, kMatFluid, {1, 1, 1});

    const T dx = converter.getPhysDeltaX();

    const Vector<T, 3> inflowExtent{dx, kPipeWidth, kPipeHeight};
    const Vector<T, 3> inflowOrigin{-dx / 2., 0., 0.};
    IndicatorCuboid3D<T> inflow(inflowExtent, inflowOrigin);
    geometry.rename(kMatWall, kMatInflow, kMatFluid, inflow);

    const Vector<T, 3> outflowExtent{2. * dx, kPipeWidth, kPipeHeight};
    const Vector<T, 3> outflowOrigin{kPipeLength - 1.5 * dx, 0., 0.};
    IndicatorCuboid3D<T> outflow(outflowExtent, outflowOrigin);
    geometry.rename(kMatWall, kMatOutflow, kMatFluid, outflow);

    const Vector<T, 3> ventExtent{kVentWidthX, 2. * dx, kVentHeightZ};
    const Vector<T, 3> ventOrigin{kVentCenterX - kVentWidthX * 0.5,
                                  -dx / 2.,
                                  kVentCenterZ - kVentHeightZ * 0.5};
    IndicatorCuboid3D<T> leftVent(ventExtent, ventOrigin);
    geometry.rename(kMatWall, kMatSideVent, kMatFluid, leftVent);

    geometry.clean();
    geometry.innerClean();
    geometry.checkForErrors();

    materializeStats = wxbgi_openlb_materialize_super_geometry_3d(geometry);
    geometry.getStatistics().print();
}

void prepareLattice(const UnitConverter<T, DESCRIPTOR> &converter,
                    SuperGeometry<T, 3> &geometry,
                    SuperLattice<T, DESCRIPTOR> &lattice)
{
    OstreamManager clout(std::cout, "prepareLattice3D");
    clout << "Preparing 3D lattice..." << std::endl;

    lattice.defineDynamics<BGKdynamics>(geometry, kMatFluid);
    lattice.defineDynamics<BGKdynamics>(geometry, kMatInflow);
    lattice.defineDynamics<BGKdynamics>(geometry, kMatOutflow);
    lattice.defineDynamics<BGKdynamics>(geometry, kMatSideVent);

    boundary::set<boundary::BounceBack>(lattice, geometry, kMatWall);
    boundary::set<boundary::BounceBack>(lattice, geometry, kMatSieve);
    boundary::set<boundary::InterpolatedVelocity>(lattice, geometry, kMatInflow);
    boundary::set<boundary::InterpolatedPressure>(lattice, geometry, kMatOutflow);
    boundary::set<boundary::InterpolatedPressure>(lattice, geometry, kMatSideVent);

    AnalyticalConst3D<T, T> rhoF(1);
    const std::vector<T> zeroVelocity(3, T(0));
    AnalyticalConst3D<T, T> uF(zeroVelocity);

    for (int material : {kMatFluid, kMatInflow, kMatOutflow, kMatSideVent})
    {
        lattice.defineRhoU(geometry, material, rhoF, uF);
        lattice.iniEquilibrium(geometry, material, rhoF, uF);
    }

    lattice.setParameter<descriptors::OMEGA>(converter.getLatticeRelaxationFrequency());
    lattice.initialize();
}

void updateBoundaryValues(std::size_t iT,
                          const UnitConverter<T, DESCRIPTOR> &converter,
                          SuperGeometry<T, 3> &geometry,
                          SuperLattice<T, DESCRIPTOR> &lattice)
{
    if (iT % 4 != 0)
        return;

    const T ramp = (iT >= kRampSteps)
                       ? T(1)
                       : static_cast<T>(iT) / static_cast<T>(kRampSteps);
    const T smoothRamp = ramp * ramp * (3. - 2. * ramp);

    std::vector<T> maxVelocity(3, T(0));
    maxVelocity[0] = kInflowPeakFactor * converter.getCharLatticeVelocity() * smoothRamp;

    const T distanceToWall = converter.getPhysDeltaX() * 0.5;
    RectanglePoiseuille3D<T> poiseuille(geometry,
                                        kMatInflow,
                                        maxVelocity,
                                        distanceToWall,
                                        distanceToWall,
                                        distanceToWall);
    momenta::setVelocity(lattice, geometry.getMaterialIndicator(kMatInflow), poiseuille);
    lattice.setProcessingContext<Array<momenta::FixedVelocityMomentumGeneric::VELOCITY>>(
        ProcessingContext::Simulation);
}

int sampleMaterialAt(const SuperGeometry<T, 3> &geometry, T x, T y, T z)
{
    const auto latticeR = geometry.getCuboidDecomposition().getLatticeR(Vector<T, 3>{x, y, z});
    if (!latticeR)
        return -1;
    return geometry.get(*latticeR);
}

bool allFinite(const T *values, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (!std::isfinite(static_cast<double>(values[i])))
            return false;
    }
    return true;
}

float sampleLongitudinalSection(SuperLattice<T, DESCRIPTOR> &lattice,
                                const UnitConverter<T, DESCRIPTOR> &converter,
                                const SuperGeometry<T, 3> &geometry,
                                int sliceZ,
                                int cols,
                                int rows,
                                std::vector<float> &scalar,
                                std::vector<float> &vectors)
{
    lattice.setProcessingContext(ProcessingContext::Evaluation);
    SuperLatticePhysVelocity3D<T, DESCRIPTOR> velocity(lattice, converter);

    float maxMagnitude = 0.f;
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            const std::size_t scalarIdx = static_cast<std::size_t>(y * cols + x);
            const std::size_t vectorIdx = scalarIdx * 2;

            scalar[scalarIdx] = 0.f;
            vectors[vectorIdx + 0] = 0.f;
            vectors[vectorIdx + 1] = 0.f;

            const int material = geometry.get(0, x, y, sliceZ);
            if (material != kMatFluid && material != kMatInflow &&
                material != kMatOutflow &&
                material != kMatSideVent)
                continue;

            T output[3] = {};
            const int input[4] = {0, x, y, sliceZ};
            if (!velocity(output, input))
                continue;
            if (!allFinite(output, 3))
                continue;

            const float vx = static_cast<float>(output[0]);
            const float vy = static_cast<float>(output[1]);
            const float vz = static_cast<float>(output[2]);
            const float magnitude = std::sqrt(vx * vx + vy * vy + vz * vz);
            if (!std::isfinite(magnitude))
                continue;

            scalar[scalarIdx] = magnitude;
            vectors[vectorIdx + 0] = vx;
            vectors[vectorIdx + 1] = vy;
            maxMagnitude = std::max(maxMagnitude, magnitude);
        }
    }

    return maxMagnitude;
}

int paletteColorForScalar(float value, float maxValue)
{
    static const int kColors[] = {BLUE, CYAN, LIGHTCYAN, GREEN, LIGHTGREEN, YELLOW, LIGHTRED, RED, WHITE};
    const float norm = (maxValue > 1e-6f) ? std::clamp(value / maxValue, 0.f, 1.f) : 0.f;
    const std::size_t idx = static_cast<std::size_t>(norm * static_cast<float>(std::size(kColors) - 1));
    return kColors[idx];
}

void rebuildFlowPerspectiveScene(const UnitConverter<T, DESCRIPTOR> &converter,
                                 int cols,
                                 int rows,
                                 const std::vector<float> &scalar,
                                 float maxMagnitude,
                                 float sliceZPhys)
{
    const float dx = static_cast<float>(converter.getPhysDeltaX());
    const float baseZ = sliceZPhys;
    const float heightScale = 0.18f;

    wxbgi_dds_scene_set_active("flow3d");
    wxbgi_dds_scene_clear("flow3d");
    wxbgi_cam_set_active("pipe3d_flow3d");

    setcolor(DARKGRAY);
    wxbgi_world_line(0.f, 0.f, baseZ, static_cast<float>(kPipeLength), 0.f, baseZ);
    wxbgi_world_line(static_cast<float>(kPipeLength), 0.f, baseZ,
                     static_cast<float>(kPipeLength), static_cast<float>(kPipeWidth), baseZ);
    wxbgi_world_line(static_cast<float>(kPipeLength), static_cast<float>(kPipeWidth), baseZ,
                     0.f, static_cast<float>(kPipeWidth), baseZ);
    wxbgi_world_line(0.f, static_cast<float>(kPipeWidth), baseZ, 0.f, 0.f, baseZ);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols - 1; ++x)
        {
            const std::size_t idx0 = static_cast<std::size_t>(y * cols + x);
            const std::size_t idx1 = idx0 + 1;
            const float x0 = (static_cast<float>(x) + 0.5f) * dx;
            const float x1 = (static_cast<float>(x + 1) + 0.5f) * dx;
            const float yw = (static_cast<float>(y) + 0.5f) * dx;
            const float z0 = baseZ + (maxMagnitude > 1e-6f ? scalar[idx0] / maxMagnitude : 0.f) * heightScale;
            const float z1 = baseZ + (maxMagnitude > 1e-6f ? scalar[idx1] / maxMagnitude : 0.f) * heightScale;
            setcolor(paletteColorForScalar(0.5f * (scalar[idx0] + scalar[idx1]), maxMagnitude));
            wxbgi_world_line(x0, yw, z0, x1, yw, z1);
        }
    }

    for (int x = 0; x < cols; ++x)
    {
        for (int y = 0; y < rows - 1; ++y)
        {
            const std::size_t idx0 = static_cast<std::size_t>(y * cols + x);
            const std::size_t idx1 = idx0 + static_cast<std::size_t>(cols);
            const float xw = (static_cast<float>(x) + 0.5f) * dx;
            const float y0 = (static_cast<float>(y) + 0.5f) * dx;
            const float y1 = (static_cast<float>(y + 1) + 0.5f) * dx;
            const float z0 = baseZ + (maxMagnitude > 1e-6f ? scalar[idx0] / maxMagnitude : 0.f) * heightScale;
            const float z1 = baseZ + (maxMagnitude > 1e-6f ? scalar[idx1] / maxMagnitude : 0.f) * heightScale;
            setcolor(paletteColorForScalar(0.5f * (scalar[idx0] + scalar[idx1]), maxMagnitude));
            wxbgi_world_line(xw, y0, z0, xw, y1, z1);
        }
    }

    for (int y = 0; y < rows; y += 2)
    {
        for (int x = 0; x < cols; x += 4)
        {
            const std::size_t idx = static_cast<std::size_t>(y * cols + x);
            const float xw = (static_cast<float>(x) + 0.5f) * dx;
            const float yw = (static_cast<float>(y) + 0.5f) * dx;
            const float zh = baseZ + (maxMagnitude > 1e-6f ? scalar[idx] / maxMagnitude : 0.f) * heightScale;
            setcolor(paletteColorForScalar(scalar[idx], maxMagnitude));
            wxbgi_world_line(xw, yw, baseZ, xw, yw, zh);
        }
    }

    wxbgi_dds_scene_set_active("default");
    wxbgi_cam_set_active("pipe3d_preview");
}

void drawHud(int sliceZ,
              float sliceZPhys,
              int sliceMin,
              int sliceMax,
              int hudLeft,
              int hudTop,
              int hudHeight,
              float maxMagnitude,
              float maxPhysVelocity,
              std::size_t steps,
               const WxbgiOpenLbMaterializeStats &materializeStats,
               int renderMode,
               const SieveLayout &sieveLayout)
{
    const int lineStep = 22;
    int lineY = hudTop;
    auto drawBullet = [&](const char *text, int color = LIGHTGRAY)
    {
        setcolor(color);
        outtextxy(hudLeft, lineY, const_cast<char *>("-"));
        outtextxy(hudLeft + 14, lineY, const_cast<char *>(text));
        lineY += lineStep;
    };

    setcolor(DARKGRAY);
    rectangle(hudLeft - 10, hudTop - 8, hudLeft + kHudPanelWidth - 4, hudTop + hudHeight - 8);
    setcolor(WHITE);
    outtextxy(hudLeft, lineY, const_cast<char *>("3D OpenLB duct HUD"));
    lineY += lineStep + 4;
    drawBullet("row1 orbit preview");
    drawBullet("row2 XY slice");
    drawBullet("row3 3D slice mesh");
    lineY += 4;

    char line[160] = {};
    std::snprintf(line, sizeof(line), "slice z: %d of %d", sliceZ, sliceMax);
    drawBullet(line, WHITE);
    std::snprintf(line, sizeof(line), "slice z: %.3f m", sliceZPhys);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "slice keys: + / -");
    drawBullet(line);
    std::snprintf(line, sizeof(line), "slice min/max:");
    drawBullet(line);
    std::snprintf(line, sizeof(line), "%d .. %d", sliceMin, sliceMax);
    drawBullet(line);
    lineY += 4;

    std::snprintf(line, sizeof(line), "top shading 1/2/3");
    drawBullet(line, WHITE);
    std::snprintf(line, sizeof(line), "1 wire 2 flat");
    drawBullet(line);
    std::snprintf(line, sizeof(line), "3 smooth");
    drawBullet(line);
    std::snprintf(line, sizeof(line), "active: %s", renderModeLabel(renderMode));
    drawBullet(line);
    lineY += 4;

    drawBullet("legend = speed", WHITE);
    drawBullet("units are m/s");
    drawBullet("legend min 0.0");
    std::snprintf(line, sizeof(line), "legend max %.5f", maxMagnitude);
    drawBullet(line);
    lineY += 4;

    std::snprintf(line, sizeof(line), "sieve grid %dx%d",
                  static_cast<int>(sieveLayout.holeYs.size()),
                  static_cast<int>(sieveLayout.holeZs.size()));
    drawBullet(line, WHITE);
    std::snprintf(line, sizeof(line), "hole dia %.1f mm",
                  static_cast<double>(kSieveHoleDiameter * 1000.0));
    drawBullet(line);
    std::snprintf(line, sizeof(line), "DDS hits %d",
                  materializeStats.matched);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "updated cells %d",
                  materializeStats.updated);
    drawBullet(line);
    lineY += 4;

    drawBullet("bottom orbit I/J/K/L", WHITE);
    drawBullet("Esc quits demo");
    drawBullet("one wall vent");
    std::snprintf(line, sizeof(line), "steps %zu", steps);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "slice max %.5f", maxMagnitude);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "flow max %.5f", maxPhysVelocity);
    drawBullet(line);
}

} // namespace

int main(int argc, char **argv)
{
    bool testMode = false;
    int solidMode = kModeSmooth;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--test") == 0)
            testMode = true;
        else if (std::strcmp(argv[i], "--wireframe") == 0)
            solidMode = kModeWireframe;
        else if (std::strcmp(argv[i], "--flat") == 0)
            solidMode = kModeFlat;
        else if (std::strcmp(argv[i], "--shaded") == 0 || std::strcmp(argv[i], "--smooth") == 0)
            solidMode = kModeSmooth;
    }

    initialize(&argc, &argv);
    singleton::directories().setOutputDir("./tmp/wxbgi_openlb_pipe_3d/");

    const UnitConverterFromResolutionAndLatticeVelocity<T, DESCRIPTOR> converter(
        kResolution,
        kCharLatticeU,
        kCharPhysLength,
        kCharPhysVelocity,
        kPhysViscosity,
        kPhysDensity);

    const int expectedX = static_cast<int>(std::ceil(kPipeLength / converter.getPhysDeltaX()));
    const int expectedY = static_cast<int>(std::ceil(kPipeWidth / converter.getPhysDeltaX()));
    const int fieldGridW = expectedX * kFieldCellPx + 12 + kLegendWidthPx;
    const int windowW = std::max(1520, kWindowMargin * 3 + fieldGridW + kHudPanelWidth + 220);
    const int row2Top = kPreviewTop + kPreviewHeight + kPanelGap;
    const int row2Height = expectedY * kFieldCellPx + 32;
    const int row3Top = row2Top + row2Height + kPanelGap + 28;
    const int windowH = row3Top + kFlowPanelHeight + kWindowMargin;

    wxbgi_openlb_begin_session(windowW, windowH, "wx_bgi OpenLB 3D Duct Demo");
    setbkcolor(BLACK);
    cleardevice();
    const SieveLayout sieveLayout = buildDdsPipeScene(converter);
    wxbgi_cam_create("pipe3d_preview", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("pipe3d_preview", "default");
    wxbgi_dds_scene_create("flow3d");
    wxbgi_cam_create("pipe3d_flow3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("pipe3d_flow3d", "flow3d");
    wxbgi_cam_set_perspective("pipe3d_flow3d", 34.f, 0.05f, 12.f);
    float flowCamAzimuthDeg = kFlowCamAzimuthDefaultDeg;
    float flowCamElevationDeg = kFlowCamElevationDefaultDeg;
    updateFlowCameraOrbit(flowCamAzimuthDeg, flowCamElevationDeg);

    applyRenderMode(solidMode);
    wxbgi_solid_set_face_color(LIGHTGRAY);
    wxbgi_solid_set_edge_color(DARKGRAY);

    const Vector<T, 3> ductExtent{kPipeLength, kPipeWidth, kPipeHeight};
    const Vector<T, 3> ductOrigin;
    IndicatorCuboid3D<T> duct(ductExtent, ductOrigin);
    CuboidDecomposition<T, 3> cuboidDecomposition(duct, converter.getPhysDeltaX(), 1);
    HeuristicLoadBalancer<T> loadBalancer(cuboidDecomposition);
    SuperGeometry<T, 3> geometry(cuboidDecomposition, loadBalancer);

    WxbgiOpenLbMaterializeStats materializeStats;
    prepareGeometry(converter, geometry, materializeStats);

    require(materializeStats.matched > 0, "3D sieve did not hit any lattice cells");
    require(sampleMaterialAt(geometry, kSieveX, 0.03, 0.03) == kMatSieve,
            "3D sieve solid sample did not materialize");
    require(sampleMaterialAt(geometry, kSieveX, sieveLayout.holeYs.front(), sieveLayout.holeZs.front()) == kMatFluid,
            "3D sieve hole sample should stay fluid");
    require(sampleMaterialAt(geometry, kVentCenterX, 0.0, kVentCenterZ) == kMatSideVent,
            "Side vent was not created");

    SuperLattice<T, DESCRIPTOR> lattice(converter, geometry);
    prepareLattice(converter, geometry, lattice);

    const int sliceCols = lattice.getBlock(0).getNx();
    const int sliceRows = lattice.getBlock(0).getNy();
    const int latticeZ = lattice.getBlock(0).getNz();
    std::vector<float> scalar(static_cast<std::size_t>(sliceCols * sliceRows), 0.f);
    std::vector<float> vectors(static_cast<std::size_t>(sliceCols * sliceRows * 2), 0.f);

    const int maxFrames = testMode ? 2 : 0;
    const int stepsPerFrame = testMode ? 704 : 2;
    const int sliceIndexMin = 1;
    const int sliceIndexMax = std::max(sliceIndexMin, latticeZ - 2);
    int sliceZ = std::clamp(latticeZ / 2, sliceIndexMin, sliceIndexMax);

    std::size_t iT = 0;
    float maxObserved = 0.f;
    std::size_t frame = 0;
    InputLatch inputLatch;
    while ((testMode ? static_cast<int>(frame) < maxFrames : true) && wxbgi_openlb_pump())
    {
        if (handleRealtimeInput(inputLatch, solidMode, flowCamAzimuthDeg, flowCamElevationDeg,
                                sliceZ, sliceIndexMin, sliceIndexMax))
            goto finished;

        while (wxbgi_key_pressed())
        {
            const int key = wxbgi_read_key();
            if (key == 27)
                goto finished;
        }

        for (int step = 0; step < stepsPerFrame; ++step, ++iT)
        {
            if (!testMode || (step % 16) == 0)
            {
                if (wxbgi_poll_events() < 0)
                    goto finished;
                if (handleRealtimeInput(inputLatch, solidMode, flowCamAzimuthDeg, flowCamElevationDeg,
                                        sliceZ, sliceIndexMin, sliceIndexMax))
                    goto finished;
            }
            updateBoundaryValues(iT, converter, geometry, lattice);
            lattice.collideAndStream();
        }

        const float sliceValueMax = std::max(sampleLongitudinalSection(lattice,
                                                                       converter,
                                                                       geometry,
                                                                       sliceZ,
                                                                       sliceCols,
                                                                       sliceRows,
                                                                       scalar,
                                                                       vectors),
                                              1e-6f);
        const T latticeMaxU = lattice.getStatistics().getMaxU();
        if (testMode)
            require(std::isfinite(static_cast<double>(latticeMaxU)) && latticeMaxU < T(0.2),
                    "3D OpenLB flow diverged");
        const float maxPhysVelocity = std::isfinite(static_cast<double>(latticeMaxU))
            ? static_cast<float>(converter.getPhysVelocity(latticeMaxU))
            : 0.f;
        if (std::isfinite(maxPhysVelocity))
            maxObserved = std::max(maxObserved, maxPhysVelocity);
        const float sliceZPhys = static_cast<float>(converter.getPhysLength(sliceZ));
        const bool renderFrame = !testMode || (static_cast<int>(frame) + 1 == maxFrames);
        if (renderFrame)
        {
            const DemoLayout layout = computeLayout(sliceCols, sliceRows);
            updatePreviewCameraOrbit(frame, layout.previewW);
            wxbgi_cam_set_screen_viewport("pipe3d_flow3d",
                                          layout.flowLeft,
                                          layout.flowTop,
                                          layout.flowW,
                                          kFlowPanelHeight);
            rebuildFlowPerspectiveScene(converter, sliceCols, sliceRows, scalar, sliceValueMax, sliceZPhys);

            cleardevice();
            wxbgi_render_dds("pipe3d_preview");
            wxbgi_field_draw_scalar_grid(layout.fieldLeft, layout.fieldTop,
                                         sliceCols, sliceRows,
                                         scalar.data(), static_cast<int>(scalar.size()),
                                         kFieldCellPx,
                                         0.f, sliceValueMax,
                                         WXBGI_FIELD_PALETTE_TURBO);
            wxbgi_field_draw_vector_grid(layout.fieldLeft, layout.fieldTop,
                                         sliceCols, sliceRows,
                                         vectors.data(), static_cast<int>(vectors.size()),
                                         kFieldCellPx,
                                         12.f, 3, WHITE);
            wxbgi_field_draw_scalar_legend(layout.legendLeft,
                                           layout.fieldTop,
                                           kLegendWidthPx,
                                           std::max(48, sliceRows * kFieldCellPx - 32),
                                           0.f, sliceValueMax,
                                           WXBGI_FIELD_PALETTE_TURBO,
                                           "speed");
            wxbgi_render_dds("pipe3d_flow3d");
            drawHud(sliceZ, sliceZPhys, sliceIndexMin, sliceIndexMax, layout.hudLeft, layout.hudTop, layout.hudH, sliceValueMax, maxPhysVelocity, iT,
                    materializeStats, solidMode, sieveLayout);

            if (!wxbgi_openlb_present())
                break;
        }
        ++frame;
    }

finished:
    if (testMode)
    {
        require(iT >= 1300, "3D OpenLB test mode did not reach the stability window");
        require(iT > 0, "3D OpenLB loop did not advance");
        require(maxObserved > 0.f, "3D OpenLB flow never accelerated");
    }
    std::printf("wxbgi_openlb_pipe_3d_demo: 3D duct animation OK\n");
    return 0;
}
