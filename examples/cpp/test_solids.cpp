/**
 * @file test_solids.cpp
 * @brief Phase 4/5/6 automated ctest — 3D solids, surfaces, and extrusion.
 *
 * Opens a 640×480 window, creates a perspective camera, draws all eight new
 * solid types, then validates:
 *  - DDS object count is at least 11.
 *  - JSON serialization contains expected type tokens.
 *  - Round-trip JSON serialize/deserialize preserves the object count.
 *  - Re-rendering in solid mode does not crash.
 *
 * Exit code 0 = pass, 1 = fail.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_solid.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_solids]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

bool contains(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    constexpr int kW = 640, kH = 480;
    initwindow(kW, kH, "test_solids", 0, 0, 0, 0);
    require(graphresult() == 0, "initwindow failed");

    // -----------------------------------------------------------------------
    // Set up a perspective camera looking at the scene from (150,150,120).
    // -----------------------------------------------------------------------
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 60.f, 1.f, 5000.f);
    wxbgi_cam_set_eye   ("cam3d", 150.f, 150.f, 120.f);
    wxbgi_cam_set_target("cam3d",   0.f,   0.f,   0.f);
    wxbgi_cam_set_up    ("cam3d",   0.f,   0.f,   1.f);
    wxbgi_cam_set_screen_viewport("cam3d", 0, 0, kW, kH);
    wxbgi_cam_set_active("cam3d");

    // -----------------------------------------------------------------------
    // Configure wireframe rendering with white edges.
    // -----------------------------------------------------------------------
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_WIREFRAME);
    require(wxbgi_solid_get_draw_mode() == WXBGI_SOLID_WIREFRAME,
            "wxbgi_solid_get_draw_mode should return WIREFRAME");
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_set_face_color(bgi::LIGHTGRAY);

    // -----------------------------------------------------------------------
    // Phase 4 — draw five solid primitives.
    // -----------------------------------------------------------------------

    // Box at origin.
    wxbgi_solid_box(0.f, 0.f, 0.f, 40.f, 40.f, 40.f);

    // Sphere offset along X.
    wxbgi_solid_sphere(80.f, 0.f, 0.f, 20.f, 12, 8);

    // Cylinder offset along −X.
    wxbgi_solid_cylinder(-80.f, 0.f, 0.f, 15.f, 40.f, 12, 1);

    // Cone offset along Y.
    wxbgi_solid_cone(0.f, 80.f, 0.f, 15.f, 40.f, 12, 1);

    // Torus offset along −Y.
    wxbgi_solid_torus(0.f, -80.f, 0.f, 20.f, 6.f, 16, 8);

    // -----------------------------------------------------------------------
    // Phase 5 — height-map and parametric surface.
    // -----------------------------------------------------------------------

    // 4×4 height-map.
    float heights[16] = {
         0.f,  5.f,  5.f,  0.f,
         5.f, 10.f, 10.f,  5.f,
         5.f, 10.f, 10.f,  5.f,
         0.f,  5.f,  5.f,  0.f
    };
    wxbgi_surface_heightmap(-30.f, -30.f, -20.f, 20.f, 20.f, 4, 4, heights);

    // Saddle (hyperbolic paraboloid) parametric surface.
    wxbgi_surface_parametric(60.f, 60.f, 0.f,
                             WXBGI_PARAM_SADDLE, 30.f, 2.f, 10, 10);

    // -----------------------------------------------------------------------
    // Phase 6 — extrude a triangle.
    // -----------------------------------------------------------------------
    float xs[] = { 0.f, 30.f, 15.f };
    float ys[] = { 0.f,  0.f, 26.f };
    wxbgi_extrude_polygon(xs, ys, 3, 0.f, 0.f, 30.f, 1, 1);

    // -----------------------------------------------------------------------
    // Validate DDS object count.
    // DDS should contain at minimum:
    //   default camera + world UCS + cam3d camera = 3 config objects
    //   5 solids + 1 heightmap + 1 param_surface + 1 extrusion = 8 drawing objects
    //   Total >= 11.
    // -----------------------------------------------------------------------
    const int totalCount = wxbgi_dds_object_count();
    if (totalCount < 11)
    {
        std::fprintf(stderr,
                     "FAIL [test_solids]: expected >= 11 objects, got %d\n",
                     totalCount);
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // Validate JSON serialization.
    // -----------------------------------------------------------------------
    const std::string json = wxbgi_dds_to_json();
    require(!json.empty(),                   "JSON should not be empty");
    require(contains(json, "\"Box\""),       "JSON should contain Box");
    require(contains(json, "\"Sphere\""),    "JSON should contain Sphere");
    require(contains(json, "\"Cylinder\""),  "JSON should contain Cylinder");
    require(contains(json, "\"Cone\""),      "JSON should contain Cone");
    require(contains(json, "\"Torus\""),     "JSON should contain Torus");
    require(contains(json, "\"HeightMap\""), "JSON should contain HeightMap");
    require(contains(json, "\"ParamSurface\""), "JSON should contain ParamSurface");
    require(contains(json, "\"Extrusion\""), "JSON should contain Extrusion");

    // -----------------------------------------------------------------------
    // Switch to solid mode and re-render — must not crash.
    // -----------------------------------------------------------------------
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    require(wxbgi_solid_get_draw_mode() == WXBGI_SOLID_SOLID,
            "wxbgi_solid_get_draw_mode should return SOLID");

    cleardevice();
    wxbgi_render_dds("cam3d");

    // -----------------------------------------------------------------------
    // Round-trip serialize / deserialize and verify object count is preserved.
    // -----------------------------------------------------------------------
    const int countBeforeRoundtrip = wxbgi_dds_object_count();
    const std::string jsonSnapshot  = wxbgi_dds_to_json();

    const int rc = wxbgi_dds_from_json(jsonSnapshot.c_str());
    require(rc == 0, "wxbgi_dds_from_json should return 0 on success");

    const int countAfterRoundtrip = wxbgi_dds_object_count();
    if (countAfterRoundtrip != countBeforeRoundtrip)
    {
        std::fprintf(stderr,
                     "FAIL [test_solids]: object count changed during JSON round-trip "
                     "(%d → %d)\n",
                     countBeforeRoundtrip, countAfterRoundtrip);
        std::exit(1);
    }

    // Final re-render through cam3d after round-trip (must not crash).
    cleardevice();
    wxbgi_render_dds("cam3d");

    closegraph();
    std::printf("PASS [test_solids]\n");
    return 0;
}
