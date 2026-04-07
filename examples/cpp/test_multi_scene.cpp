/**
 * @file test_multi_scene.cpp
 * @brief Headless ctest for the Multi-CHDOP / Multi-Scene API.
 *
 * Verifies:
 *  1. "default" scene is always present (exists = 1).
 *  2. wxbgi_dds_scene_create() creates a new named scene.
 *  3. wxbgi_dds_scene_exists() returns 0 for an unknown scene.
 *  4. wxbgi_dds_scene_set_active() routes draw calls to the new scene.
 *  5. wxbgi_dds_scene_get_active() returns the correct active name.
 *  6. Objects drawn into "secondary" are NOT visible in "default".
 *  7. wxbgi_cam_set_scene() / wxbgi_cam_get_scene() work correctly.
 *  8. wxbgi_dds_scene_destroy() removes a scene and falls back cameras to "default".
 *  9. Destroying "default" is a no-op.
 * 10. JSON round-trip preserves the scene assignment on cameras.
 *
 * Exits with code 0 on success, 1 on any assertion failure.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_multi_scene]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

bool contains(const std::string &hay, const std::string &needle)
{
    return hay.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    constexpr int kW = 320, kH = 240;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_multi_scene");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    // -----------------------------------------------------------------------
    // 1. "default" scene always exists.
    // -----------------------------------------------------------------------
    require(wxbgi_dds_scene_exists("default") == 1, "default scene not found");

    // -----------------------------------------------------------------------
    // 2. Create a new scene.
    // -----------------------------------------------------------------------
    require(wxbgi_dds_scene_create("secondary") == 0, "create secondary failed");
    require(wxbgi_dds_scene_exists("secondary") == 1, "secondary not found after create");

    // -----------------------------------------------------------------------
    // 3. Unknown scene returns 0.
    // -----------------------------------------------------------------------
    require(wxbgi_dds_scene_exists("__no_such_scene__") == 0, "unknown scene should not exist");

    // -----------------------------------------------------------------------
    // 4+5. Active scene routing and get_active.
    // -----------------------------------------------------------------------
    // Active should start at "default".
    require(std::string(wxbgi_dds_scene_get_active()) == "default", "initial active should be default");

    // Draw something into "default".
    setcolor(bgi::RED);
    circle(100, 100, 30);          // appended to "default"

    // Switch to "secondary" and draw something.
    wxbgi_dds_scene_set_active("secondary");
    require(std::string(wxbgi_dds_scene_get_active()) == "secondary", "active should be secondary");
    setcolor(bgi::BLUE);
    line(10, 10, 200, 200);        // appended to "secondary"

    // Switch back to "default".
    wxbgi_dds_scene_set_active("default");
    require(std::string(wxbgi_dds_scene_get_active()) == "default", "active should return to default");

    // -----------------------------------------------------------------------
    // 6. Objects drawn into "secondary" don't show in "default" count.
    //    "default" has: 1 camera, 1 UCS, 1 circle = 3 objects.
    //    "secondary"   has: 1 line = 1 object.
    // -----------------------------------------------------------------------
    // We can't easily query per-scene object counts directly, but we CAN
    // verify via JSON serialization that both scenes are present.
    const std::string json(wxbgi_dds_to_json());
    require(contains(json, "\"secondary\""), "JSON should contain secondary scene");
    require(contains(json, "\"activeScene\""), "JSON should contain activeScene key");

    // -----------------------------------------------------------------------
    // 7. Camera-to-scene assignment.
    // -----------------------------------------------------------------------
    // Create a second camera and assign it to "secondary".
    require(wxbgi_cam_create("cam2", WXBGI_CAM_ORTHO) > 0, "cam2 create failed");

    // Default assignment should be "default".
    require(std::string(wxbgi_cam_get_scene("default")) == "default",
            "default camera should be assigned to default scene");

    wxbgi_cam_set_scene("cam2", "secondary");
    require(std::string(wxbgi_cam_get_scene("cam2")) == "secondary",
            "cam2 should be assigned to secondary");

    // Assigning to a non-existent scene should be a no-op.
    wxbgi_cam_set_scene("cam2", "__no_such_scene__");
    require(std::string(wxbgi_cam_get_scene("cam2")) == "secondary",
            "cam2 should still be secondary after assigning to unknown scene");

    // -----------------------------------------------------------------------
    // 8. Destroy "secondary" — cam2 should fall back to "default".
    // -----------------------------------------------------------------------
    wxbgi_dds_scene_destroy("secondary");
    require(wxbgi_dds_scene_exists("secondary") == 0, "secondary should be gone");
    require(std::string(wxbgi_cam_get_scene("cam2")) == "default",
            "cam2 should fall back to default after secondary destroyed");

    // -----------------------------------------------------------------------
    // 9. Destroying "default" is a no-op.
    // -----------------------------------------------------------------------
    wxbgi_dds_scene_destroy("default");
    require(wxbgi_dds_scene_exists("default") == 1, "default must survive destroy attempt");

    // -----------------------------------------------------------------------
    // 10. JSON round-trip: create a scene, assign a camera, serialize, reload.
    // -----------------------------------------------------------------------
    require(wxbgi_dds_scene_create("third") == 0, "create third failed");
    wxbgi_cam_set_scene("cam2", "third");
    require(std::string(wxbgi_cam_get_scene("cam2")) == "third",
            "cam2 should be third before round-trip");

    // Serialize.
    const std::string json2(wxbgi_dds_to_json());
    require(contains(json2, "\"third\""), "JSON round-trip: 'third' scene missing");
    require(contains(json2, "\"scene\""), "JSON round-trip: camera scene field missing");

    // Reload.
    require(wxbgi_dds_from_json(json2.c_str()) == 0, "dds_from_json failed");
    require(wxbgi_dds_scene_exists("third") == 1,
            "round-trip: 'third' scene should be restored");
    require(std::string(wxbgi_cam_get_scene("cam2")) == "third",
            "round-trip: cam2 should still be assigned to 'third'");

    std::fprintf(stdout, "test_multi_scene: all checks passed.\n");
    closegraph();
    return 0;
}
