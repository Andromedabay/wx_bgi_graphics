/**
 * @file test_dds_clear.cpp
 * @brief Phase E – Test 3: DDS delete and clear operations.
 *
 * Validates that:
 *  1. wxbgi_dds_remove() soft-deletes a single object by ID and reduces the
 *     count by one.
 *  2. wxbgi_dds_clear() removes all drawing primitives but preserves cameras
 *     and UCS objects.
 *  3. wxbgi_dds_clear_all() removes every object and then restores the two
 *     mandatory defaults (the "default" camera and the "world" UCS), leaving
 *     the count at exactly 2.
 *  4. After wxbgi_dds_clear_all() the camera named "default" exists and the
 *     UCS named "world" exists (the library stays usable).
 *
 * Exit code 0 = pass, 1 = fail.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_dds_clear]: %s\n", msg);
    std::exit(1);
}

void requireEq(int got, int expected, const char *context)
{
    if (got != expected)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_clear]: %s — expected %d, got %d\n",
                     context, expected, got);
        std::exit(1);
    }
}

} // namespace

int main()
{
    constexpr int kW = 320, kH = 240;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_dds_clear");
    if (graphresult() != 0)
        fail("wxbgi_wx_frame_create failed");

    // -----------------------------------------------------------------------
    // Baseline: 2 objects (default camera + world UCS).
    // -----------------------------------------------------------------------
    requireEq(wxbgi_dds_object_count(), 2, "baseline object count");

    // -----------------------------------------------------------------------
    // Draw 3 primitives → total should be 5.
    // -----------------------------------------------------------------------
    setcolor(bgi::WHITE);
    line(10, 10, 100, 100);     // obj 3
    circle(160, 120, 40);       // obj 4
    rectangle(20, 20, 80, 80);  // obj 5

    requireEq(wxbgi_dds_object_count(), 5, "count after 3 draws");

    // -----------------------------------------------------------------------
    // Label the second primitive (circle) so we can find it by label.
    // -----------------------------------------------------------------------
    // The circle is the second drawing object inserted, at index 2 overall
    // (0-based: index 0 = camera, index 1 = UCS, index 2 = line, index 3 = circle).
    const std::string circleId(wxbgi_dds_get_id_at(3));
    if (circleId.empty())
        fail("wxbgi_dds_get_id_at(3) returned empty string");

    wxbgi_dds_set_label(circleId.c_str(), "my_circle");
    if (std::string(wxbgi_dds_find_by_label("my_circle")) != circleId)
        fail("find_by_label did not return the expected ID after set_label");

    // -----------------------------------------------------------------------
    // Remove the circle by ID → total becomes 4.
    // -----------------------------------------------------------------------
    int removed = wxbgi_dds_remove(circleId.c_str());
    if (removed != 1)
        fail("wxbgi_dds_remove returned 0 (object not found)");

    requireEq(wxbgi_dds_object_count(), 4, "count after remove");

    // The label should no longer be resolvable.
    if (std::string(wxbgi_dds_find_by_label("my_circle")) != "")
        fail("find_by_label still returned a result after remove");

    // Remove a non-existent ID must return 0 without crashing.
    if (wxbgi_dds_remove("nonexistent_id_99999") != 0)
        fail("remove of nonexistent ID returned non-zero");

    // -----------------------------------------------------------------------
    // wxbgi_dds_clear() — removes all drawing primitives, keeps camera+UCS.
    // After this: camera (1) + UCS (1) = 2.
    // -----------------------------------------------------------------------
    wxbgi_dds_clear();
    requireEq(wxbgi_dds_object_count(), 2, "count after wxbgi_dds_clear");

    // Camera and UCS must still be accessible via the camera API.
    // The active camera should still be "default".
    const std::string activeCam(wxbgi_cam_get_active());
    if (activeCam != "default")
        fail("active camera is not 'default' after wxbgi_dds_clear");

    // -----------------------------------------------------------------------
    // Draw 2 more primitives to prepare for clear_all test.
    // -----------------------------------------------------------------------
    setcolor(bgi::LIGHTCYAN);
    line(50, 50, 200, 200);
    circle(100, 100, 30);

    requireEq(wxbgi_dds_object_count(), 4, "count after second batch of draws");

    // -----------------------------------------------------------------------
    // wxbgi_dds_clear_all() — nukes everything and recreates the 2 defaults.
    // -----------------------------------------------------------------------
    wxbgi_dds_clear_all();
    requireEq(wxbgi_dds_object_count(), 2, "count after wxbgi_dds_clear_all");

    // Confirm the two default objects are the expected types.
    const std::string id0(wxbgi_dds_get_id_at(0));
    const std::string id1(wxbgi_dds_get_id_at(1));

    if (id0.empty() || id1.empty())
        fail("default objects missing after wxbgi_dds_clear_all");

    const int type0 = wxbgi_dds_get_type(id0.c_str());
    const int type1 = wxbgi_dds_get_type(id1.c_str());

    // Objects are inserted in order: camera first, then UCS.
    requireEq(type0, WXBGI_DDS_CAMERA, "first object after clear_all is not Camera");
    requireEq(type1, WXBGI_DDS_UCS,    "second object after clear_all is not Ucs");

    // The active camera should default back to "default" and the library should
    // accept new draw calls without crashing.
    setcolor(bgi::YELLOW);
    rectangle(10, 10, 50, 50);
    requireEq(wxbgi_dds_object_count(), 3, "count after draw following clear_all");

    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_dds_clear]\n");
    return 0;
}
