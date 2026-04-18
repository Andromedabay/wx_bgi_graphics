/**
 * @file test_dds_csg.cpp
 * @brief Regression coverage for retained DDS transform/union/intersection nodes.
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
    std::fprintf(stderr, "FAIL [test_dds_csg]: %s\n", msg);
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

bool hasAnyNonBlackNear(int x, int y, int radius)
{
    for (int yy = y - radius; yy <= y + radius; ++yy)
        for (int xx = x - radius; xx <= x + radius; ++xx)
            if (xx >= 0 && yy >= 0 && xx < getmaxx() && yy < getmaxy() &&
                getpixel(xx, yy) != bgi::BLACK)
                return true;
    return false;
}

bool hasAnyNonBlack()
{
    for (int yy = 0; yy < getmaxy(); ++yy)
        for (int xx = 0; xx < getmaxx(); ++xx)
            if (getpixel(xx, yy) != bgi::BLACK)
                return true;
    return false;
}

std::string lastId()
{
    const int count = wxbgi_dds_object_count();
    require(count > 0, "DDS unexpectedly empty");
    const char *id = wxbgi_dds_get_id_at(count - 1);
    require(id != nullptr && id[0] != '\0', "Failed to fetch last DDS id");
    return std::string(id);
}

void resetScene()
{
    wxbgi_dds_clear();
    cleardevice();
}

} // namespace

int main()
{
    constexpr int kW = 160;
    constexpr int kH = 120;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_dds_csg");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    // ---------------------------------------------------------------------
    // Translate via retained transform node.
    // ---------------------------------------------------------------------
    resetScene();
    setfillstyle(bgi::SOLID_FILL, bgi::WHITE);
    setcolor(bgi::WHITE);
    bar(20, 20, 40, 40);
    const std::string barId = lastId();

    const std::string txId = wxbgi_dds_translate(barId.c_str(), 30.f, 0.f, 0.f);
    require(!txId.empty(), "wxbgi_dds_translate returned empty id");
    require(wxbgi_dds_get_type(txId.c_str()) == WXBGI_DDS_TRANSFORM,
            "Translate did not create a Transform object");
    require(wxbgi_dds_get_child_count(txId.c_str()) == 1,
            "Transform child count mismatch");
    require(std::string(wxbgi_dds_get_child_at(txId.c_str(), 0)) == barId,
            "Transform child id mismatch");

    cleardevice();
    wxbgi_render_dds(nullptr);
    require(getpixel(60, 30) == bgi::WHITE, "Translated bar not rendered at target location");
    require(getpixel(30, 30) == bgi::BLACK, "Original bar remained visible after translate test");

    std::string json = wxbgi_dds_to_json();
    require(contains(json, "\"type\": \"Transform\""), "JSON missing Transform object");

    // ---------------------------------------------------------------------
    // Union via retained set-op node.
    // ---------------------------------------------------------------------
    resetScene();
    setfillstyle(bgi::SOLID_FILL, bgi::GREEN);
    setcolor(bgi::GREEN);
    bar(20, 20, 50, 50);
    const std::string unionA = lastId();
    bar(40, 20, 70, 50);
    const std::string unionB = lastId();
    const char *unionIds[2] = {unionA.c_str(), unionB.c_str()};
    const std::string unionId = wxbgi_dds_union(2, unionIds);
    require(!unionId.empty(), "wxbgi_dds_union returned empty id");
    require(wxbgi_dds_get_type(unionId.c_str()) == WXBGI_DDS_SET_UNION,
            "Union did not create a SetUnion object");
    require(wxbgi_dds_get_child_count(unionId.c_str()) == 2,
            "Union child count mismatch");
    require(wxbgi_object_set_face_color(unionId.c_str(), bgi::LIGHTMAGENTA) == 1,
            "Failed to recolor retained union object");
    cleardevice();
    wxbgi_render_dds(nullptr);
    require(getpixel(25, 35) == bgi::LIGHTMAGENTA, "Union missed left-only area");
    require(getpixel(65, 35) == bgi::LIGHTMAGENTA, "Union missed right-only area");
    require(getpixel(10, 10) == bgi::BLACK, "Union polluted background");

    json = wxbgi_dds_to_json();
    require(contains(json, "\"type\": \"SetUnion\""), "JSON missing SetUnion object");
    require(contains(json, "\"faceColor\": 13"), "Union faceColor update missing from JSON");
    wxbgi_dds_set_solid_draw_mode(WXBGI_SOLID_WIREFRAME);
    json = wxbgi_dds_to_json();
    require(contains(json, "\"drawMode\": 0"), "DDS draw-mode update did not propagate to SetUnion");

    // ---------------------------------------------------------------------
    // Intersection via retained set-op node.
    // ---------------------------------------------------------------------
    resetScene();
    setfillstyle(bgi::SOLID_FILL, bgi::CYAN);
    setcolor(bgi::CYAN);
    bar(20, 20, 50, 50);
    const std::string isectA = lastId();
    bar(40, 20, 70, 50);
    const std::string isectB = lastId();
    const char *isectIds[2] = {isectA.c_str(), isectB.c_str()};
    const std::string isectId = wxbgi_dds_intersection(2, isectIds);
    require(!isectId.empty(), "wxbgi_dds_intersection returned empty id");
    require(wxbgi_dds_get_type(isectId.c_str()) == WXBGI_DDS_SET_INTERSECTION,
            "Intersection did not create a SetIntersection object");
    cleardevice();
    wxbgi_render_dds(nullptr);
    require(getpixel(45, 35) == bgi::CYAN, "Intersection missed overlap");
    require(getpixel(25, 35) == bgi::BLACK, "Intersection leaked left-only area");
    require(getpixel(65, 35) == bgi::BLACK, "Intersection leaked right-only area");

    json = wxbgi_dds_to_json();
    require(contains(json, "\"type\": \"SetIntersection\""), "JSON missing SetIntersection object");

    // ---------------------------------------------------------------------
    // Difference via retained set-op node.
    // ---------------------------------------------------------------------
    resetScene();
    setfillstyle(bgi::SOLID_FILL, bgi::LIGHTRED);
    setcolor(bgi::LIGHTRED);
    bar(20, 20, 70, 50);
    const std::string diffA = lastId();
    bar(40, 20, 70, 50);
    const std::string diffB = lastId();
    const char *diffIds[2] = {diffA.c_str(), diffB.c_str()};
    const std::string diffId = wxbgi_dds_difference(2, diffIds);
    require(!diffId.empty(), "wxbgi_dds_difference returned empty id");
    require(wxbgi_dds_get_type(diffId.c_str()) == WXBGI_DDS_SET_DIFFERENCE,
            "Difference did not create a SetDifference object");
    cleardevice();
    wxbgi_render_dds(nullptr);
    require(getpixel(30, 35) == bgi::LIGHTRED, "Difference missed base-only area");
    require(getpixel(55, 35) == bgi::BLACK, "Difference failed to subtract overlap");

    json = wxbgi_dds_to_json();
    require(contains(json, "\"type\": \"SetDifference\""), "JSON missing SetDifference object");

    // ---------------------------------------------------------------------
    // 3D intersection must be exact geometry, not screen-space mask overlap.
    // Two boxes separated along X have identical side-view silhouettes from
    // a camera looking down the X axis, so a mask backend would show pixels
    // while an exact 3D boolean must be empty.
    // ---------------------------------------------------------------------
    wxbgi_set_legacy_gl_render(1);
    resetScene();
    wxbgi_cam_create("csg_side", WXBGI_CAM_ORTHO);
    wxbgi_cam_set_eye("csg_side", 200.f, 0.f, 0.f);
    wxbgi_cam_set_target("csg_side", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up("csg_side", 0.f, 0.f, 1.f);
    wxbgi_cam_set_ortho_auto("csg_side", 120.f, -500.f, 500.f);
    wxbgi_cam_set_screen_viewport("csg_side", 0, 0, kW, kH);
    wxbgi_cam_set_active("csg_side");

    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_set_face_color(bgi::WHITE);
    wxbgi_solid_box(0.f, 0.f, 0.f, 20.f, 20.f, 20.f);
    const std::string boxA = lastId();
    wxbgi_solid_box(60.f, 0.f, 0.f, 20.f, 20.f, 20.f);
    const std::string boxB = lastId();
    const char *solidIds[2] = {boxA.c_str(), boxB.c_str()};
    const std::string solidIsectId = wxbgi_dds_intersection(2, solidIds);
    require(!solidIsectId.empty(), "3D wxbgi_dds_intersection returned empty id");
    float sx = -1.f, sy = -1.f;
    require(wxbgi_cam_world_to_screen("csg_side", 0.f, 0.f, 0.f, &sx, &sy) == 1,
            "Failed to project 3D intersection sample point");
    cleardevice();
    wxbgi_render_dds("csg_side");
    require(getpixel(static_cast<int>(sx), static_cast<int>(sy)) == bgi::BLACK,
            "3D intersection fell back to screen-space overlap instead of exact boolean");

    resetScene();
    wxbgi_cam_create("csg_diff_side", WXBGI_CAM_ORTHO);
    wxbgi_cam_set_eye("csg_diff_side", 200.f, 0.f, 0.f);
    wxbgi_cam_set_target("csg_diff_side", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up("csg_diff_side", 0.f, 0.f, 1.f);
    wxbgi_cam_set_ortho_auto("csg_diff_side", 120.f, -500.f, 500.f);
    wxbgi_cam_set_screen_viewport("csg_diff_side", 0, 0, kW, kH);
    wxbgi_cam_set_active("csg_diff_side");
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_set_face_color(bgi::WHITE);
    wxbgi_solid_box(0.f, 0.f, 0.f, 24.f, 24.f, 24.f);
    const std::string diffBoxA = lastId();
    wxbgi_solid_box(8.f, 0.f, 0.f, 24.f, 30.f, 30.f);
    const std::string diffBoxB = lastId();
    const char *solidDiffIds[2] = {diffBoxA.c_str(), diffBoxB.c_str()};
    const std::string solidDiffId = wxbgi_dds_difference(2, solidDiffIds);
    require(!solidDiffId.empty(), "3D wxbgi_dds_difference returned empty id");
    cleardevice();
    wxbgi_render_dds("csg_diff_side");
    require(hasAnyNonBlack(),
            "3D difference fell back to screen-space subtraction instead of exact boolean");

    // ---------------------------------------------------------------------
    // 3D cylinder difference must render the actual remaining volume, not the
    // full outer cylinder. A top-down orthographic camera should see an annulus
    // (ring): outer radius filled, center hole empty.
    // ---------------------------------------------------------------------
    resetScene();
    wxbgi_cam_create("csg_diff_top", WXBGI_CAM_ORTHO);
    wxbgi_cam_set_eye("csg_diff_top", 0.f, 0.f, 200.f);
    wxbgi_cam_set_target("csg_diff_top", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up("csg_diff_top", 0.f, 1.f, 0.f);
    wxbgi_cam_set_ortho_auto("csg_diff_top", 120.f, -500.f, 500.f);
    wxbgi_cam_set_screen_viewport("csg_diff_top", 0, 0, kW, kH);
    wxbgi_cam_set_active("csg_diff_top");
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(bgi::WHITE);
    wxbgi_solid_set_face_color(bgi::WHITE);
    wxbgi_solid_cylinder(0.f, 0.f, 0.f, 24.f, 30.f, 18, 1);
    const std::string ringOuter = lastId();
    wxbgi_solid_cylinder(0.f, 0.f, 0.f, 14.f, 30.f, 18, 1);
    const std::string ringInner = lastId();
    const char *ringIds[2] = {ringOuter.c_str(), ringInner.c_str()};
    const std::string ringDiffId = wxbgi_dds_difference(2, ringIds);
    require(!ringDiffId.empty(), "Top-view cylinder difference returned empty id");

    float ringSx = -1.f, ringSy = -1.f;
    float holeSx = -1.f, holeSy = -1.f;
    require(wxbgi_cam_world_to_screen("csg_diff_top", 20.f, 0.f, 0.f, &ringSx, &ringSy) == 1,
            "Failed to project ring sample point");
    require(wxbgi_cam_world_to_screen("csg_diff_top", 0.f, 0.f, 0.f, &holeSx, &holeSy) == 1,
            "Failed to project hole sample point");

    cleardevice();
    wxbgi_render_dds("csg_diff_top");
    require(getpixel(static_cast<int>(ringSx), static_cast<int>(ringSy)) != bgi::BLACK,
            "3D difference missed the remaining annulus");
    require(getpixel(static_cast<int>(holeSx), static_cast<int>(holeSy)) == bgi::BLACK,
            "3D difference rendered the removed inner cylinder instead of a hole");

    wxbgi_set_legacy_gl_render(0);
    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_dds_csg]\n");
    return 0;
}
