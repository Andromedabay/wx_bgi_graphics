#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"
#include "wx_bgi_solid.h"
#include "wx_bgi_wx.h"

#include <olb.h>
#include <wx/gbsizer.h>
#include <wx/settings.h>
#include <wx/slider.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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

constexpr int k_repaintEveryNthFrame = 16;

constexpr T kPipeLength      = 1.80;
constexpr T kPipeWidth       = 0.36;
constexpr T kPipeHeight      = 0.24;
constexpr T kWallThickness   = 0.03;
constexpr T kSieveX          = 0.15;
constexpr T kSieveThickness  = 0.03;
constexpr T kSieveHoleDiameterDefault = 0.018;
constexpr T kSievePitchFactor = 2.0;
constexpr T kVentCenterX     = 0.95;
constexpr T kVentWidthX      = 0.14;
constexpr T kVentHeightZ     = 0.08;
constexpr T kVentCenterZ     = kPipeHeight * 0.5;
constexpr T kCharPhysLength  = 0.10;
constexpr T kCharPhysVelocityDefault = 0.06;
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
constexpr std::size_t kVtkExportDefaultIterations = 100;
constexpr T kInflowPeakFactor = 1.5;
constexpr int kModeWireframe = WXBGI_SOLID_WIREFRAME;
constexpr int kModeFlat      = WXBGI_SOLID_FLAT;
constexpr int kModeSmooth    = WXBGI_SOLID_SMOOTH;
constexpr float kFlowCamAzimuthDefaultDeg = -118.f;
constexpr float kFlowCamElevationDefaultDeg = 26.f;
constexpr float kFlowCamOrbitRadius = 1.55f;
constexpr float kFlowCamStepDeg = 6.f;
constexpr int kControlPanelWidth = 128;
constexpr int kFlowSliderScale = 1000;
// constexpr int kFlowSliderMin = 10;
// constexpr int kFlowSliderMax = 120;
constexpr int kFlowSliderMin = 0;    // 0.000
constexpr int kFlowSliderMax = 100;  // 0.100 (safe upper limit for stability with the given geometry and viscosity)

struct SieveLayout
{
    std::vector<float> holeYs;
    std::vector<float> holeZs;
    T requestedHoleDiameter = kSieveHoleDiameterDefault;
    T effectiveHoleY = kSieveHoleDiameterDefault;
    T effectiveHoleZ = kSieveHoleDiameterDefault;
    T solidSampleY = 0.0;
    T solidSampleZ = 0.0;
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

struct VtkExportState
{
    SuperVTMwriter<T, 3> writer;
    SuperLatticePhysPressure3D<T, DESCRIPTOR> pressure;
    SuperLatticePhysVelocity3D<T, DESCRIPTOR> velocity;
    bool enabled = false;
    std::size_t maxIterations = 0;

    VtkExportState(SuperLattice<T, DESCRIPTOR> &lattice,
                   const UnitConverter<T, DESCRIPTOR> &converter,
                   bool enableExport,
                   std::size_t maxExportIterations)
        : writer("wxbgi_openlb_pipe_3d"),
          pressure(lattice, converter),
          velocity(lattice, converter),
          enabled(enableExport),
          maxIterations(maxExportIterations)
    {
        if (!enabled)
            return;

        writer.addFunctor(pressure, "pressure");
        writer.addFunctor(velocity, "velocity");
        writer.createMasterFile();
    }

    void write(std::size_t completedIteration)
    {
        if (!enabled || completedIteration == 0 || completedIteration > maxIterations)
            return;
        writer.write(static_cast<int>(completedIteration));
    }
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

std::size_t parsePositiveSizeArg(const char *value, const char *option)
{
    require(value != nullptr && *value != '\0', option);

    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    require(end != nullptr && *end == '\0', option);
    require(parsed > 0, option);
    return static_cast<std::size_t>(parsed);
}

T parsePositiveRealArg(const char *value, const char *option)
{
    require(value != nullptr && *value != '\0', option);

    char *end = nullptr;
    const double parsed = std::strtod(value, &end);
    require(end != nullptr && *end == '\0', option);
    require(std::isfinite(parsed) && parsed > 0.0, option);
    return static_cast<T>(parsed);
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

SieveLayout buildDdsPipeScene(const UnitConverter<T, DESCRIPTOR> &converter,
                              T requestedHoleDiameter)
{
    wxbgi_dds_scene_set_active("default");
    wxbgi_dds_clear();
    SieveLayout layout;
    layout.requestedHoleDiameter = requestedHoleDiameter;

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

    const T dx = converter.getPhysDeltaX();
    const auto sieveCountForAxis = [dx](T requested, T extent) -> int
    {
        const int maxCount = std::max(1, static_cast<int>(std::floor(extent / dx - 1e-6)) - 1);
        const int scaledCount = std::max(1, static_cast<int>(std::floor(extent /
                                                                         (requested * kSievePitchFactor))));
        return std::clamp(scaledCount, 1, maxCount);
    };
    const auto snappedHoleForAxis = [dx](T requested, T extent, int holeCount) -> T
    {
        const T cellsAcross = extent / dx;
        const int maxHoleCells = std::max(1, static_cast<int>(std::floor((cellsAcross - 1e-6) /
                                                                          static_cast<T>(holeCount))));
        const int requestedHoleCells = std::max(1, static_cast<int>(std::llround(requested / dx)));
        return static_cast<T>(std::clamp(requestedHoleCells, 1, maxHoleCells)) * dx;
    };

    const int sieveHoleCols = sieveCountForAxis(requestedHoleDiameter, kPipeWidth);
    const int sieveHoleRows = sieveCountForAxis(requestedHoleDiameter, kPipeHeight);
    const T holeY = snappedHoleForAxis(requestedHoleDiameter, kPipeWidth, sieveHoleCols);
    const T holeZ = snappedHoleForAxis(requestedHoleDiameter, kPipeHeight, sieveHoleRows);
    layout.effectiveHoleY = holeY;
    layout.effectiveHoleZ = holeZ;
    const T barY = (kPipeWidth - static_cast<T>(sieveHoleCols) * holeY) /
                   static_cast<T>(sieveHoleCols + 1);
    const T barZ = (kPipeHeight - static_cast<T>(sieveHoleRows) * holeZ) /
                   static_cast<T>(sieveHoleRows + 1);
    require(barY > 0.0 && barZ > 0.0, "Sieve lattice alignment produced a non-positive bar width");
    layout.solidSampleY = barY * 0.5;
    layout.solidSampleZ = barZ * 0.5;

    std::vector<std::string> sieveBars;
    sieveBars.reserve(static_cast<std::size_t>(sieveHoleCols + sieveHoleRows + 2));

    T yCursor = barY;
    for (int i = 0; i < sieveHoleCols; ++i)
    {
        layout.holeYs.push_back(static_cast<float>(yCursor + holeY * 0.5));
        yCursor += holeY + barY;
    }

    T zCursor = barZ;
    for (int i = 0; i < sieveHoleRows; ++i)
    {
        layout.holeZs.push_back(static_cast<float>(zCursor + holeZ * 0.5));
        zCursor += holeZ + barZ;
    }

    T barCenterY = barY * 0.5;
    for (int i = 0; i <= sieveHoleCols; ++i)
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
    for (int i = 0; i <= sieveHoleRows; ++i)
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
                          SuperLattice<T, DESCRIPTOR> &lattice,
                          T requestedPhysVelocity)
{
    if (iT % 4 != 0)
        return;

    const T ramp = (iT >= kRampSteps)
                       ? T(1)
                       : static_cast<T>(iT) / static_cast<T>(kRampSteps);
    const T smoothRamp = ramp * ramp * (3. - 2. * ramp);

    std::vector<T> maxVelocity(3, T(0));
    maxVelocity[0] = kInflowPeakFactor * converter.getLatticeVelocity(requestedPhysVelocity) * smoothRamp;

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
              float requestedFlowVelocity,
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
    std::snprintf(line, sizeof(line), "hole req %.1f mm",
                  static_cast<double>(sieveLayout.requestedHoleDiameter * 1000.0));
    drawBullet(line);
    std::snprintf(line, sizeof(line), "hole eff %.1f x %.1f mm",
                  static_cast<double>(sieveLayout.effectiveHoleY * 1000.0),
                  static_cast<double>(sieveLayout.effectiveHoleZ * 1000.0));
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
    drawBullet("left slider = flow", WHITE);
    std::snprintf(line, sizeof(line), "flow req %.3f m/s", requestedFlowVelocity);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "steps %zu", steps);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "slice max %.5f", maxMagnitude);
    drawBullet(line);
    std::snprintf(line, sizeof(line), "flow max %.5f", maxPhysVelocity);
    drawBullet(line);
}

} // namespace

struct DemoOptions
{
    int solidMode = kModeSmooth;
    bool testMode = false;
    bool vtkExportEnabled = false;
    std::size_t vtkExportIterations = kVtkExportDefaultIterations;
    T sieveHoleDiameter = kSieveHoleDiameterDefault;
    T charPhysVelocity = kCharPhysVelocityDefault;
};

DemoOptions parseDemoOptions(int argc, char **argv)
{
    DemoOptions options;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--test") == 0)
            options.testMode = true;
        else if (std::strcmp(argv[i], "--wireframe") == 0)
            options.solidMode = kModeWireframe;
        else if (std::strcmp(argv[i], "--flat") == 0)
            options.solidMode = kModeFlat;
        else if (std::strcmp(argv[i], "--shaded") == 0 || std::strcmp(argv[i], "--smooth") == 0)
            options.solidMode = kModeSmooth;
        else if (std::strcmp(argv[i], "--vtk") == 0)
            options.vtkExportEnabled = true;
        else if (std::strncmp(argv[i], "--vtk=", 6) == 0)
        {
            options.vtkExportEnabled = true;
            options.vtkExportIterations = parsePositiveSizeArg(argv[i] + 6,
                                                               "--vtk expects a positive integer iteration count");
        }
        else if (std::strcmp(argv[i], "--vtk-iterations") == 0)
        {
            require(i + 1 < argc, "--vtk-iterations requires a positive integer argument");
            options.vtkExportEnabled = true;
            options.vtkExportIterations = parsePositiveSizeArg(argv[++i],
                                                               "--vtk-iterations requires a positive integer argument");
        }
        else if (std::strncmp(argv[i], "--vtk-iterations=", 17) == 0)
        {
            options.vtkExportEnabled = true;
            options.vtkExportIterations = parsePositiveSizeArg(argv[i] + 17,
                                                               "--vtk-iterations expects a positive integer value");
        }
        else if (std::strcmp(argv[i], "--sieve-hole-mm") == 0)
        {
            require(i + 1 < argc, "--sieve-hole-mm requires a positive numeric argument");
            options.sieveHoleDiameter = parsePositiveRealArg(argv[++i],
                                                             "--sieve-hole-mm requires a positive numeric argument") / 1000.0;
        }
        else if (std::strncmp(argv[i], "--sieve-hole-mm=", 16) == 0)
        {
            options.sieveHoleDiameter = parsePositiveRealArg(argv[i] + 16,
                                                             "--sieve-hole-mm expects a positive numeric value") / 1000.0;
        }
        else if (std::strcmp(argv[i], "--flow-velocity-ms") == 0 || std::strcmp(argv[i], "--flow-ms") == 0)
        {
            require(i + 1 < argc, "--flow-velocity-ms requires a positive numeric argument");
            options.charPhysVelocity = parsePositiveRealArg(argv[++i],
                                                            "--flow-velocity-ms requires a positive numeric argument");
        }
        else if (std::strncmp(argv[i], "--flow-velocity-ms=", 19) == 0)
        {
            options.charPhysVelocity = parsePositiveRealArg(argv[i] + 19,
                                                            "--flow-velocity-ms expects a positive numeric value");
        }
        else if (std::strncmp(argv[i], "--flow-ms=", 10) == 0)
        {
            options.charPhysVelocity = parsePositiveRealArg(argv[i] + 10,
                                                            "--flow-ms expects a positive numeric value");
        }
    }
    return options;
}

wxSize computeRenderCanvasSize(const UnitConverter<T, DESCRIPTOR> &converter)
{
    const int expectedX = static_cast<int>(std::ceil(kPipeLength / converter.getPhysDeltaX()));
    const int expectedY = static_cast<int>(std::ceil(kPipeWidth / converter.getPhysDeltaX()));
    const int fieldGridW = expectedX * kFieldCellPx + 12 + kLegendWidthPx;
    const int canvasW = std::max(1520, kWindowMargin * 3 + fieldGridW + kHudPanelWidth + 220);
    const int row2Top = kPreviewTop + kPreviewHeight + kPanelGap;
    const int row2Height = expectedY * kFieldCellPx + 32;
    const int row3Top = row2Top + row2Height + kPanelGap + 28;
    const int canvasH = row3Top + kFlowPanelHeight + kWindowMargin;
    return wxSize(canvasW, canvasH);
}

int flowVelocityToSlider(double value)
{
    return std::clamp(static_cast<int>(std::lround(value * static_cast<T>(kFlowSliderScale))),
                      kFlowSliderMin,
                      kFlowSliderMax);
}

T sliderToFlowVelocity(int sliderValue)
{
    return static_cast<double>(sliderValue) / static_cast<T>(kFlowSliderScale);
}

class PipeDemoCanvas : public wxbgi::WxBgiCanvas
{
public:
    explicit PipeDemoCanvas(wxWindow *parent, std::function<void(int, int)> renderFn)
        : wxbgi::WxBgiCanvas(parent, wxID_ANY), m_renderFn(std::move(renderFn))
    {}

protected:
    void PreBlit(int w, int h) override
    {
        if (m_renderFn)
            m_renderFn(w, h);
    }

private:
    std::function<void(int, int)> m_renderFn;
};

enum
{
    ID_TIMER = wxID_HIGHEST + 200,
    ID_FLOW_SLIDER
};

class PipeDemoFrame : public wxFrame
{
public:
    explicit PipeDemoFrame(const DemoOptions &options);

private:
    void BuildUi(const wxSize &canvasSize);
    void InitializeSimulation();
    void UpdateFlowLabel();
    void AdvanceSimulation();
    void RenderCompositeFrame(int w, int h);
    void OnTimer(wxTimerEvent &);
    void OnFlowSlider(wxCommandEvent &);
    void OnFrameSize(wxSizeEvent &evt);
    void OnClose(wxCloseEvent &evt);
    void UpdateFrameCustom();

    DemoOptions m_options;
    PipeDemoCanvas *m_canvas{nullptr};
    wxPanel *m_canvasHost{nullptr};
    wxPanel *m_controlPanel{nullptr};
    wxPanel *m_wrapperPanel{nullptr};
    wxSlider *m_flowSlider{nullptr};
    wxStaticText *m_flowValue{nullptr};
    wxTimer m_timer;

    T m_requestedFlowVelocity;
    UnitConverterFromResolutionAndLatticeVelocity<T, DESCRIPTOR> m_converter;
    std::unique_ptr<CuboidDecomposition<T, 3>> m_cuboidDecomposition;
    std::unique_ptr<HeuristicLoadBalancer<T>> m_loadBalancer;
    std::unique_ptr<SuperGeometry<T, 3>> m_geometry;
    std::unique_ptr<SuperLattice<T, DESCRIPTOR>> m_lattice;
    std::unique_ptr<VtkExportState> m_vtkExport;

    SieveLayout m_sieveLayout;
    WxbgiOpenLbMaterializeStats m_materializeStats{};
    std::vector<float> m_scalar;
    std::vector<float> m_vectors;

    int m_sliceCols = 0;
    int m_sliceRows = 0;
    int m_sliceIndexMin = 1;
    int m_sliceIndexMax = 1;
    int m_sliceZ = 1;

    float m_flowCamAzimuthDeg = kFlowCamAzimuthDefaultDeg;
    float m_flowCamElevationDeg = kFlowCamElevationDefaultDeg;
    float m_sliceValueMax = 1e-6f;
    float m_maxPhysVelocity = 0.f;
    float m_sliceZPhys = 0.f;
    float m_maxObserved = 0.f;
    std::size_t m_iT = 0;
    std::size_t m_frame = 0;
    InputLatch m_inputLatch;
    bool m_reportedSuccess = false;
};

void PipeDemoFrame::UpdateFrameCustom()
{
        m_canvas->Refresh();
        m_canvas->Update();
        m_wrapperPanel->Refresh();
        m_wrapperPanel->Update();
        m_canvasHost->Refresh();
        m_canvasHost->Update();
        m_controlPanel->Refresh();
        m_controlPanel->Update();
        this->Refresh();
        this->Update();
        m_canvas->SetFocus();
}


PipeDemoFrame::PipeDemoFrame(const DemoOptions &options)
    : wxFrame(nullptr, wxID_ANY, "wx_bgi OpenLB 3D Duct Demo (wx slider)",
              wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN),
      m_options(options),
      m_timer(this, ID_TIMER),
      m_requestedFlowVelocity(options.charPhysVelocity),
      m_converter(kResolution,
                  kCharLatticeU,
                  kCharPhysLength,
                  options.charPhysVelocity,
                  kPhysViscosity,
                  kPhysDensity)
{
    const wxSize canvasSize = computeRenderCanvasSize(m_converter);
    BuildUi(canvasSize);
    wxbgi_openlb_begin_canvas_session(canvasSize.GetWidth(), canvasSize.GetHeight());
    //setbkcolor(BLACK);
    //cleardevice();
    InitializeSimulation();
    UpdateFrameCustom();

    Bind(wxEVT_TIMER, &PipeDemoFrame::OnTimer, this, ID_TIMER);
    Bind(wxEVT_SIZE, &PipeDemoFrame::OnFrameSize, this);
    Bind(wxEVT_CLOSE_WINDOW, &PipeDemoFrame::OnClose, this);
    CallAfter([this]() {
        //Layout();
        // if (m_controlPanel)
        // {
        //     m_controlPanel->Refresh();
        //     //m_controlPanel->Update();
        // }
        if (m_canvas)
        {
            m_canvas->Refresh(false);
            //m_canvas->Update();
        }
        if (m_wrapperPanel)
        {
            m_wrapperPanel->Refresh();
            //m_wrapperPanel->Update();
        }
        if (m_canvasHost)
        {
            m_canvasHost->Refresh();
            //m_canvasHost->Update();
        }
        // this->Update();
    });
    m_timer.Start(16);
}

void PipeDemoFrame::BuildUi(const wxSize &canvasSize)
{
    const auto margin = FromDIP(2);
    //auto *mainSizer = new wxBoxSizer(wxHORIZONTAL);

    //auto *root = new wxGridBagSizer(margin, margin);
    auto *root = new wxBoxSizer(wxHORIZONTAL);
    const wxColour panelBg = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
    const wxColour panelFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    const int LEFT_WIDTH = kControlPanelWidth;

    m_controlPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxBORDER_SIMPLE | wxCLIP_CHILDREN);
    m_controlPanel->SetBackgroundColour(panelBg);
    m_controlPanel->SetForegroundColour(panelFg);
    m_controlPanel->SetDoubleBuffered(true);
    m_controlPanel->SetMinSize(wxSize(LEFT_WIDTH, canvasSize.GetHeight()));
    m_controlPanel->SetMaxSize(wxSize(LEFT_WIDTH, canvasSize.GetHeight()));
    auto *controls = new wxBoxSizer(wxVERTICAL);
    auto *flowLabel = new wxStaticText(m_controlPanel, wxID_ANY, "Flow");
    flowLabel->SetForegroundColour(panelFg);
    controls->Add(flowLabel, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 8);
    auto *unitsLabel = new wxStaticText(m_controlPanel, wxID_ANY, "m/s");
    unitsLabel->SetForegroundColour(panelFg);
    controls->Add(unitsLabel, 0, wxALIGN_CENTER | wxBOTTOM, 6);

    m_flowSlider = new wxSlider(m_controlPanel,
                                ID_FLOW_SLIDER,
                                flowVelocityToSlider(m_requestedFlowVelocity),
                                kFlowSliderMin,
                                kFlowSliderMax,
                                wxDefaultPosition,
                                wxSize(72, canvasSize.GetHeight() - 108),
                                wxSL_VERTICAL | wxSL_INVERSE | wxSL_LABELS);
    m_flowSlider->SetBackgroundColour(panelBg);
    m_flowSlider->SetForegroundColour(panelFg);
    m_flowSlider->SetValue(flowVelocityToSlider(m_requestedFlowVelocity));
    //std::cout << "SETVALUE CALLED: " << flowVelocityToSlider(m_requestedFlowVelocity) << std::endl;
    controls->Add(m_flowSlider, 0, wxEXPAND | wxTOP | wxBOTTOM, 8);
    m_flowValue = new wxStaticText(m_controlPanel, wxID_ANY, "");
    m_flowValue->SetForegroundColour(panelFg);
    controls->Add(m_flowValue, 0, wxALIGN_CENTER | wxALL, 8);
    m_controlPanel->SetBackgroundColour(panelBg);
    m_controlPanel->SetForegroundColour(panelFg);
    m_controlPanel->SetSizer(controls);

    m_wrapperPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE | wxCLIP_CHILDREN);
    m_wrapperPanel->SetBackgroundColour(panelBg);
    m_wrapperPanel->SetForegroundColour(panelFg);
    m_wrapperPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

    m_canvasHost = new wxPanel(m_wrapperPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN);
    m_canvasHost->SetBackgroundColour(wxColour(24, 24, 24));
    m_canvasHost->SetDoubleBuffered(true);
    auto *canvasSizer = new wxBoxSizer(wxVERTICAL);
    m_canvas = new PipeDemoCanvas(m_canvasHost, [this](int w, int h) { RenderCompositeFrame(w, h); });
    wxSize m_canvas_minsize(canvasSize.GetWidth() , canvasSize.GetHeight());
    m_canvas->SetMinSize(m_canvas_minsize);
    m_canvas->SetMaxSize(m_canvas_minsize);
    canvasSizer->Add(m_canvas, 1, wxEXPAND);
    m_canvasHost->SetSizer(canvasSizer);
    m_canvasHost->SetMinSize( m_canvas->GetMinSize() );
    m_canvasHost->SetMaxSize( m_canvas->GetMaxSize() );
        
    m_wrapperPanel->GetSizer()->Add(m_canvasHost, 0, wxEXPAND, margin);

    // root->Add(m_controlPanel, {0, 0}, {1, 1}, wxALIGN_CENTER_VERTICAL, margin);
    // root->Add(m_wrapperPanel, {0, 1}, {1, 1}, wxALIGN_CENTER_VERTICAL, margin);
    // root->AddGrowableCol(0, 1);
    // root->AddGrowableCol(1, 5);
    root->Add(m_controlPanel, 0, wxEXPAND | wxLEFT, margin);
    root->Add(m_wrapperPanel, 1, wxEXPAND | wxRIGHT, margin);

    //mainSizer->Add(root,0, wxEXPAND);
    //SetSizerAndFit(mainSizer);
    //SetSizerAndFit(root);
    this->SetSizerAndFit(root);
    //this->SetSizer(root);
    //this->Layout();
    //SetBackgroundColour(wxColour(24, 24, 24));
    UpdateFlowLabel();
    Bind(wxEVT_SCROLL_CHANGED, &PipeDemoFrame::OnFlowSlider, this, ID_FLOW_SLIDER);

    // m_canvas->Refresh();
    // m_wrapperPanel->Refresh();
    // m_canvasHost->Refresh();
    // m_controlPanel->Refresh();
    // this->Update();
}

void PipeDemoFrame::InitializeSimulation()
{
    m_sieveLayout = buildDdsPipeScene(m_converter, m_options.sieveHoleDiameter);
    wxbgi_cam_create("pipe3d_preview", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("pipe3d_preview", "default");
    wxbgi_dds_scene_create("flow3d");
    wxbgi_cam_create("pipe3d_flow3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("pipe3d_flow3d", "flow3d");
    wxbgi_cam_set_perspective("pipe3d_flow3d", 34.f, 0.05f, 12.f);
    updateFlowCameraOrbit(m_flowCamAzimuthDeg, m_flowCamElevationDeg);

    applyRenderMode(m_options.solidMode);
    wxbgi_solid_set_face_color(LIGHTGRAY);
    wxbgi_solid_set_edge_color(DARKGRAY);

    const Vector<T, 3> ductExtent{kPipeLength, kPipeWidth, kPipeHeight};
    const Vector<T, 3> ductOrigin;
    IndicatorCuboid3D<T> duct(ductExtent, ductOrigin);
    m_cuboidDecomposition = std::make_unique<CuboidDecomposition<T, 3>>(duct, m_converter.getPhysDeltaX(), 1);
    m_loadBalancer = std::make_unique<HeuristicLoadBalancer<T>>(*m_cuboidDecomposition);
    m_geometry = std::make_unique<SuperGeometry<T, 3>>(*m_cuboidDecomposition, *m_loadBalancer);
    m_geometry->setWriteIncrementalVTK(false);

    prepareGeometry(m_converter, *m_geometry, m_materializeStats);
    require(m_materializeStats.matched > 0, "3D sieve did not hit any lattice cells");
    require(sampleMaterialAt(*m_geometry, kSieveX, m_sieveLayout.solidSampleY, m_sieveLayout.solidSampleZ) == kMatSieve,
            "3D sieve solid sample did not materialize");
    require(sampleMaterialAt(*m_geometry, kSieveX, m_sieveLayout.holeYs.front(), m_sieveLayout.holeZs.front()) == kMatFluid,
            "3D sieve hole sample should stay fluid");
    require(sampleMaterialAt(*m_geometry, kVentCenterX, 0.0, kVentCenterZ) == kMatSideVent,
            "Side vent was not created");

    m_lattice = std::make_unique<SuperLattice<T, DESCRIPTOR>>(m_converter, *m_geometry);
    prepareLattice(m_converter, *m_geometry, *m_lattice);
    m_vtkExport = std::make_unique<VtkExportState>(*m_lattice,
                                                   m_converter,
                                                   m_options.vtkExportEnabled && !m_options.testMode,
                                                   m_options.vtkExportIterations);

    m_sliceCols = m_lattice->getBlock(0).getNx();
    m_sliceRows = m_lattice->getBlock(0).getNy();
    const int latticeZ = m_lattice->getBlock(0).getNz();
    m_scalar.assign(static_cast<std::size_t>(m_sliceCols * m_sliceRows), 0.f);
    m_vectors.assign(static_cast<std::size_t>(m_sliceCols * m_sliceRows * 2), 0.f);
    m_sliceIndexMin = 1;
    m_sliceIndexMax = std::max(m_sliceIndexMin, latticeZ - 2);
    m_sliceZ = std::clamp(latticeZ / 2, m_sliceIndexMin, m_sliceIndexMax);
}

void PipeDemoFrame::UpdateFlowLabel()
{
    if (!m_flowValue)
        return;
    m_flowValue->SetLabel(wxString::Format("%.3f", static_cast<double>(m_requestedFlowVelocity)));
}

void PipeDemoFrame::RenderCompositeFrame(int /*w*/, int /*h*/)
{
    if (!m_lattice)
        return;

    const DemoLayout layout = computeLayout(m_sliceCols, m_sliceRows);
    updatePreviewCameraOrbit(m_frame, layout.previewW);
    wxbgi_cam_set_screen_viewport("pipe3d_flow3d",
                                  layout.flowLeft,
                                  layout.flowTop,
                                  layout.flowW,
                                  kFlowPanelHeight);
    rebuildFlowPerspectiveScene(m_converter, m_sliceCols, m_sliceRows, m_scalar, m_sliceValueMax, m_sliceZPhys);

    cleardevice();
    wxbgi_render_dds("pipe3d_preview");
    wxbgi_field_draw_scalar_grid(layout.fieldLeft, layout.fieldTop,
                                 m_sliceCols, m_sliceRows,
                                 m_scalar.data(), static_cast<int>(m_scalar.size()),
                                 kFieldCellPx,
                                 0.f, m_sliceValueMax,
                                 WXBGI_FIELD_PALETTE_TURBO);
    wxbgi_field_draw_vector_grid(layout.fieldLeft, layout.fieldTop,
                                 m_sliceCols, m_sliceRows,
                                 m_vectors.data(), static_cast<int>(m_vectors.size()),
                                 kFieldCellPx,
                                 12.f, 3, WHITE);
    wxbgi_field_draw_scalar_legend(layout.legendLeft,
                                   layout.fieldTop,
                                   kLegendWidthPx,
                                   std::max(48, m_sliceRows * kFieldCellPx - 32),
                                   0.f, m_sliceValueMax,
                                   WXBGI_FIELD_PALETTE_TURBO,
                                   "speed");
    wxbgi_render_dds("pipe3d_flow3d");
    drawHud(m_sliceZ, m_sliceZPhys, m_sliceIndexMin, m_sliceIndexMax, layout.hudLeft, layout.hudTop, layout.hudH,
            m_sliceValueMax, m_maxPhysVelocity, static_cast<float>(m_requestedFlowVelocity), m_iT,
            m_materializeStats, m_options.solidMode, m_sieveLayout);
}


void PipeDemoFrame::AdvanceSimulation()
{
    const int maxFrames = m_options.testMode ? 2 : 0;
    const int stepsPerTick = m_options.testMode ? 704 : 2;

    if (handleRealtimeInput(m_inputLatch, m_options.solidMode, m_flowCamAzimuthDeg, m_flowCamElevationDeg,
                            m_sliceZ, m_sliceIndexMin, m_sliceIndexMax))
    {
        Close();
        return;
    }

    while (wxbgi_key_pressed())
    {
        if (wxbgi_read_key() == 27)
        {
            Close();
            return;
        }
    }

    for (int step = 0; step < stepsPerTick; ++step, ++m_iT)
    {
        if (!m_options.testMode || (step % 16) == 0)
        {
            if (handleRealtimeInput(m_inputLatch, m_options.solidMode, m_flowCamAzimuthDeg, m_flowCamElevationDeg,
                                    m_sliceZ, m_sliceIndexMin, m_sliceIndexMax))
            {
                Close();
                return;
            }
        }
        updateBoundaryValues(m_iT, m_converter, *m_geometry, *m_lattice, m_requestedFlowVelocity);
        m_lattice->collideAndStream();
        m_vtkExport->write(m_iT + 1);
    }

    m_sliceValueMax = std::max(sampleLongitudinalSection(*m_lattice,
                                                         m_converter,
                                                         *m_geometry,
                                                         m_sliceZ,
                                                         m_sliceCols,
                                                         m_sliceRows,
                                                         m_scalar,
                                                         m_vectors),
                               1e-6f);
    const T latticeMaxU = m_lattice->getStatistics().getMaxU();
    if (m_options.testMode)
        require(std::isfinite(static_cast<double>(latticeMaxU)) && latticeMaxU < T(0.2),
                "3D OpenLB flow diverged");
    m_maxPhysVelocity = std::isfinite(static_cast<double>(latticeMaxU))
        ? static_cast<float>(m_converter.getPhysVelocity(latticeMaxU))
        : 0.f;
    if (std::isfinite(m_maxPhysVelocity))
        m_maxObserved = std::max(m_maxObserved, m_maxPhysVelocity);
    m_sliceZPhys = static_cast<float>(m_converter.getPhysLength(m_sliceZ));

    m_canvas->Render();
    ++m_frame;
    if (m_frame % k_repaintEveryNthFrame == 0)
    {
        //this->UpdateFrameCustom();
        wxYield();
        m_canvas->SetFocus();
    }

    if (m_options.testMode && static_cast<int>(m_frame) >= maxFrames)
    {
        require(m_iT >= 1300, "3D OpenLB test mode did not reach the stability window");
        require(m_iT > 0, "3D OpenLB loop did not advance");
        require(m_maxObserved > 0.f, "3D OpenLB flow never accelerated");
        if (!m_reportedSuccess)
        {
            std::printf("wxbgi_openlb_pipe_3d_wx_slider_demo: 3D duct animation OK\n");
            m_reportedSuccess = true;
        }
        Close();
    }
}

void PipeDemoFrame::OnTimer(wxTimerEvent &evt)
{
    evt.Skip();
    AdvanceSimulation();
}

void PipeDemoFrame::OnFlowSlider(wxCommandEvent &evt)
{
    evt.Skip();
    m_requestedFlowVelocity = sliderToFlowVelocity(evt.GetInt());
    //std::cout << "SLIDER EVT: " << evt.GetInt() << std::endl;
    //m_flowSlider->SetValue(flowVelocityToSlider(m_requestedFlowVelocity));
    UpdateFlowLabel();
}

void PipeDemoFrame::OnFrameSize(wxSizeEvent &evt)
{
    evt.Skip();
    Layout();
    if (m_controlPanel)
    {
        m_controlPanel->Refresh();
        //m_controlPanel->Update();
    }
    if (m_canvasHost)
    {
        m_canvasHost->Refresh();
        //m_canvasHost->Update();
    }
    if (m_canvas)
    {
        m_canvas->Refresh();
        //m_canvas->Update();
    }
    if (m_wrapperPanel)
    {
        m_wrapperPanel->Refresh();
        //m_wrapperPanel->Update();
    }
    this->Refresh();
    this->Update();
}

void PipeDemoFrame::OnClose(wxCloseEvent &evt)
{
    if (m_timer.IsRunning())
        m_timer.Stop();
    if (m_options.testMode && !m_reportedSuccess)
        fail("wx slider demo closed before completing the test window");
    evt.Skip();
}

static DemoOptions gDemoOptions;

class PipeDemoApp : public wxApp
{
public:
    bool OnInit() override
    {
        singleton::directories().setOutputDir("./tmp/wxbgi_openlb_pipe_3d_wx_slider/");
        auto *frame = new PipeDemoFrame(gDemoOptions);
        frame->Show();
        frame->Refresh();
        frame->Update();
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(PipeDemoApp);

int main(int argc, char **argv)
{
    gDemoOptions = parseDemoOptions(argc, argv);
    initialize(&argc, &argv);

    if (!wxEntryStart(argc, argv))
        return 1;
    if (!wxTheApp || !wxTheApp->CallOnInit())
        return 1;

    const int rc = wxTheApp->OnRun();
    wxTheApp->OnExit();
    wxEntryCleanup();
    return rc;
}
