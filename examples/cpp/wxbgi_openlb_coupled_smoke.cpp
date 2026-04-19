#include "wx_bgi.h"
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
using DESCRIPTOR = D2Q9<>;

constexpr int   kMatFluid    = 1;
constexpr int   kMatWall     = 2;
constexpr int   kMatInflow   = 3;
constexpr int   kMatOutflow  = 4;
constexpr int   kMatObstacle = 5;

constexpr T kDomainLengthX     = 2.2;
constexpr T kDomainHeight      = 0.41;
constexpr T kCylinderRadius    = 0.05;
constexpr T kCylinderCenterX   = 0.2;
constexpr T kCylinderCenterY   = 0.2;
constexpr T kCharPhysVelocity  = 0.08;
constexpr T kPhysDensity       = 1.0;
constexpr T kPhysViscosity     = 0.0004;
constexpr T kCharLatticeU      = 0.05;
constexpr int kResolution      = 10;
constexpr int kCellSizePx      = 4;
constexpr int kLegendWidthPx   = 20;
constexpr int kHudWidthPx      = 220;
constexpr int kGridLeftPx      = 8;
constexpr int kGridTopPx       = 8;
constexpr std::size_t kRampSteps = 160;

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [wxbgi_openlb_coupled_smoke]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

void buildDdsObstacleScene()
{
    wxbgi_dds_clear();

    wxbgi_solid_sphere(static_cast<float>(kCylinderCenterX),
                       static_cast<float>(kCylinderCenterY),
                       0.f,
                       static_cast<float>(kCylinderRadius),
                       28,
                       16);
    const std::string obstacleId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!obstacleId.empty(), "Missing DDS obstacle id");

    wxbgi_dds_set_label(obstacleId.c_str(), "openlb-cylinder-obstacle");
    require(wxbgi_dds_set_external_attr(obstacleId.c_str(), "openlb.role", "solid") == 1,
            "Failed to tag DDS obstacle role");
    require(wxbgi_dds_set_external_attr(obstacleId.c_str(), "openlb.material", "5") == 1,
            "Failed to tag DDS obstacle material");
    require(wxbgi_dds_set_external_attr(obstacleId.c_str(), "openlb.boundary", "obstacle") == 1,
            "Failed to tag DDS obstacle boundary");
}

void prepareGeometry(const UnitConverter<T, DESCRIPTOR> &converter,
                     SuperGeometry<T, 2> &superGeometry,
                     WxbgiOpenLbMaterializeStats &materializeStats)
{
    OstreamManager clout(std::cout, "prepareGeometry");
    clout << "Preparing OpenLB geometry from DDS obstacle..." << std::endl;

    superGeometry.rename(0, kMatWall);
    superGeometry.rename(kMatWall, kMatFluid, {1, 1});

    Vector<T, 2> extend(kDomainLengthX, kDomainHeight);
    Vector<T, 2> origin;

    extend[0] = 2. * converter.getPhysDeltaX();
    origin[0] = -converter.getPhysDeltaX();
    IndicatorCuboid2D<T> inflow(extend, origin);
    superGeometry.rename(kMatWall, kMatInflow, kMatFluid, inflow);

    origin[0] = kDomainLengthX - converter.getPhysDeltaX();
    IndicatorCuboid2D<T> outflow(extend, origin);
    superGeometry.rename(kMatWall, kMatOutflow, kMatFluid, outflow);

    superGeometry.clean();
    superGeometry.checkForErrors();
    materializeStats = wxbgi_openlb_materialize_super_geometry_2d(superGeometry, T(0));
    superGeometry.print();
}

void prepareLattice(const UnitConverter<T, DESCRIPTOR> &converter,
                    SuperGeometry<T, 2> &superGeometry,
                    SuperLattice<T, DESCRIPTOR> &lattice)
{
    OstreamManager clout(std::cout, "prepareLattice");
    clout << "Preparing OpenLB lattice..." << std::endl;

    const T omega = converter.getLatticeRelaxationFrequency();

    lattice.defineDynamics<BGKdynamics>(superGeometry, kMatFluid);
    boundary::set<boundary::BounceBack>(lattice, superGeometry, kMatWall);
    boundary::set<boundary::BounceBack>(lattice, superGeometry, kMatObstacle);
    boundary::set<boundary::InterpolatedVelocity>(lattice, superGeometry, kMatInflow);
    boundary::set<boundary::InterpolatedPressure>(lattice, superGeometry, kMatOutflow);

    AnalyticalConst2D<T, T> rhoF(1);
    const std::vector<T> zeroVelocity(2, T(0));
    AnalyticalConst2D<T, T> uF(zeroVelocity);

    lattice.defineRhoU(superGeometry, kMatFluid, rhoF, uF);
    lattice.defineRhoU(superGeometry, kMatInflow, rhoF, uF);
    lattice.defineRhoU(superGeometry, kMatOutflow, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, kMatFluid, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, kMatInflow, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, kMatOutflow, rhoF, uF);

    lattice.setParameter<descriptors::OMEGA>(omega);
    lattice.initialize();
}

void updateBoundaryValues(std::size_t iT,
                          const UnitConverter<T, DESCRIPTOR> &converter,
                          SuperGeometry<T, 2> &superGeometry,
                          SuperLattice<T, DESCRIPTOR> &lattice)
{
    if (iT % 4 != 0)
        return;

    const T ramp = (iT >= kRampSteps)
                       ? T(1)
                       : static_cast<T>(iT) / static_cast<T>(kRampSteps);
    const T smoothRamp = ramp * ramp * (3. - 2. * ramp);
    const T maxVelocity = converter.getCharLatticeVelocity() * 1.5 * smoothRamp;
    const T distanceToWall = converter.getPhysDeltaX() * 0.5;

    Poiseuille2D<T> poiseuille(superGeometry, kMatInflow, maxVelocity, distanceToWall);
    lattice.defineU(superGeometry, kMatInflow, poiseuille);
    lattice.setProcessingContext<Array<momenta::FixedVelocityMomentumGeneric::VELOCITY>>(
        ProcessingContext::Simulation);
}

int sampleMaterialAt(const SuperGeometry<T, 2> &superGeometry, T x, T y)
{
    const auto latticeR = superGeometry.getCuboidDecomposition().getLatticeR(Vector<T, 2>{x, y});
    if (!latticeR)
        return -1;
    return superGeometry.get(*latticeR);
}

float sampleVelocityField(SuperLattice<T, DESCRIPTOR> &lattice,
                          const UnitConverter<T, DESCRIPTOR> &converter,
                          const SuperGeometry<T, 2> &superGeometry,
                          int cols,
                          int rows,
                          std::vector<float> &scalar,
                          std::vector<float> &vectors)
{
    lattice.setProcessingContext(ProcessingContext::Evaluation);
    SuperLatticePhysVelocity2D<T, DESCRIPTOR> velocity(lattice, converter);

    float maxMagnitude = 0.f;
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            const std::size_t scalarIdx = static_cast<std::size_t>(y * cols + x);
            const std::size_t vecIdx = scalarIdx * 2;

            scalar[scalarIdx] = 0.f;
            vectors[vecIdx + 0] = 0.f;
            vectors[vecIdx + 1] = 0.f;

            const int material = superGeometry.get(0, x, y);
            if (material == 0 || material == kMatWall || material == kMatObstacle)
                continue;

            T output[2] = {};
            const int input[3] = {0, x, y};
            if (!velocity(output, input))
                continue;

            const float vx = static_cast<float>(output[0]);
            const float vy = static_cast<float>(output[1]);
            const float magnitude = std::sqrt(vx * vx + vy * vy);

            scalar[scalarIdx] = magnitude;
            vectors[vecIdx + 0] = vx;
            vectors[vecIdx + 1] = vy;
            maxMagnitude = std::max(maxMagnitude, magnitude);
        }
    }

    return maxMagnitude;
}

void drawHud(int cols,
             int rows,
             std::size_t iT,
             float scalarMax,
             float maxPhysVelocity,
             const WxbgiOpenLbMaterializeStats &materializeStats)
{
    const int hudLeft = kGridLeftPx + cols * kCellSizePx + kLegendWidthPx + 22;

    static char title[] = "OpenLB 2D coupled DDS obstacle";
    static char line1[] = "DDS obstacle -> OpenLB materials -> live field view";
    static char line2[] = "material 5 comes from the retained DDS scene";

    setcolor(WHITE);
    outtextxy(hudLeft, 12, title);
    setcolor(LIGHTGRAY);
    outtextxy(hudLeft, 30, line1);
    outtextxy(hudLeft, 46, line2);

    char line[128] = {};

    std::snprintf(line, sizeof(line), "lattice: %dx%d  step: %zu", cols, rows, iT);
    outtextxy(hudLeft, 74, line);

    std::snprintf(line, sizeof(line), "DDS hits: %d  updated cells: %d",
                  materializeStats.matched, materializeStats.updated);
    outtextxy(hudLeft, 92, line);

    std::snprintf(line, sizeof(line), "max |u|: %.5f m/s", maxPhysVelocity);
    outtextxy(hudLeft, 110, line);

    std::snprintf(line, sizeof(line), "legend max: %.5f", scalarMax);
    outtextxy(hudLeft, 128, line);
}

} // namespace

int main(int argc, char **argv)
{
    const bool testMode = (argc > 1 && std::strcmp(argv[1], "--test") == 0);

    initialize(&argc, &argv);
    OstreamManager clout(std::cout, "wxbgi_openlb_coupled_smoke");

    const UnitConverterFromResolutionAndLatticeVelocity<T, DESCRIPTOR> converter(
        kResolution,
        kCharLatticeU,
        2. * kCylinderRadius,
        kCharPhysVelocity,
        kPhysViscosity,
        kPhysDensity);

    const int expectedCols = static_cast<int>(std::ceil(kDomainLengthX / converter.getPhysDeltaX()));
    const int expectedRows = static_cast<int>(std::ceil(kDomainHeight / converter.getPhysDeltaX()));
    const int windowW = kGridLeftPx + expectedCols * kCellSizePx + kLegendWidthPx + kHudWidthPx + 24;
    const int windowH = kGridTopPx + expectedRows * kCellSizePx + 24;

    wxbgi_openlb_begin_session(windowW, windowH, "wx_bgi OpenLB Coupled Smoke");
    setbkcolor(BLACK);
    cleardevice();

    buildDdsObstacleScene();

    Vector<T, 2> domainExtent(kDomainLengthX, kDomainHeight);
    Vector<T, 2> domainOrigin;
    IndicatorCuboid2D<T> domain(domainExtent, domainOrigin);
    CuboidDecomposition<T, 2> cuboidDecomposition(domain, converter.getPhysDeltaX(), 1);
    HeuristicLoadBalancer<T> loadBalancer(cuboidDecomposition);
    SuperGeometry<T, 2> superGeometry(cuboidDecomposition, loadBalancer);

    WxbgiOpenLbMaterializeStats materializeStats;
    prepareGeometry(converter, superGeometry, materializeStats);

    require(materializeStats.matched > 0, "DDS obstacle did not hit any OpenLB cells");
    require(sampleMaterialAt(superGeometry, kCylinderCenterX, kCylinderCenterY) == kMatObstacle,
            "DDS obstacle did not materialize as OpenLB obstacle material");
    require(sampleMaterialAt(superGeometry, 0.8, 0.2) == kMatFluid,
            "OpenLB fluid sample was overwritten unexpectedly");

    SuperLattice<T, DESCRIPTOR> lattice(converter, superGeometry);
    prepareLattice(converter, superGeometry, lattice);

    const int cols = lattice.getBlock(0).getNx();
    const int rows = lattice.getBlock(0).getNy();
    std::vector<float> scalar(static_cast<std::size_t>(cols * rows), 0.f);
    std::vector<float> vectors(static_cast<std::size_t>(cols * rows * 2), 0.f);

    std::size_t iT = 0;
    const int frameCount = testMode ? 4 : 1000000;
    const int stepsPerFrame = testMode ? 32 : 8;

    while (static_cast<int>(iT / static_cast<std::size_t>(stepsPerFrame)) < frameCount &&
           wxbgi_openlb_pump())
    {
        for (int step = 0; step < stepsPerFrame; ++step, ++iT)
        {
            updateBoundaryValues(iT, converter, superGeometry, lattice);
            lattice.collideAndStream();
        }

        const float scalarMax = std::max(sampleVelocityField(lattice, converter, superGeometry,
                                                             cols, rows, scalar, vectors),
                                         1e-6f);
        const float maxPhysVelocity =
            static_cast<float>(converter.getPhysVelocity(lattice.getStatistics().getMaxU()));

        cleardevice();
        wxbgi_field_draw_scalar_grid(kGridLeftPx, kGridTopPx,
                                     cols, rows,
                                     scalar.data(), static_cast<int>(scalar.size()),
                                     kCellSizePx, 0.f, scalarMax,
                                     WXBGI_FIELD_PALETTE_TURBO);
        wxbgi_field_draw_vector_grid(kGridLeftPx, kGridTopPx,
                                     cols, rows,
                                     vectors.data(), static_cast<int>(vectors.size()),
                                     kCellSizePx, 14.f, 6, WHITE);
        wxbgi_field_draw_scalar_legend(kGridLeftPx + cols * kCellSizePx + 8, kGridTopPx,
                                       kLegendWidthPx, std::max(32, rows * kCellSizePx - 48),
                                       0.f, scalarMax,
                                       WXBGI_FIELD_PALETTE_TURBO,
                                       "|u|");
        drawHud(cols, rows, iT, scalarMax, maxPhysVelocity, materializeStats);

        if (!wxbgi_openlb_present())
            break;
        if (!testMode)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    require(iT > 0, "OpenLB loop did not advance");
    clout << "Coupled OpenLB + wx_bgi simulation steps: " << iT << std::endl;
    std::printf("wxbgi_openlb_coupled_smoke: OpenLB coupled simulation OK\n");
    return 0;
}
