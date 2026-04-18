#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"
#include "wx_bgi_solid.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_openlb_bridge_materialize_2d]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

} // namespace

int main()
{
    constexpr int kW = 320;
    constexpr int kH = 240;

    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_openlb_bridge_materialize_2d");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    cleardevice();

    wxbgi_solid_box(0.f, 0.f, 0.f, 10.f, 10.f, 2.f);
    const std::string boxId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!boxId.empty(), "Missing box id");

    wxbgi_solid_sphere(0.f, 0.f, 0.f, 2.f, 16, 12);
    const std::string sphereId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!sphereId.empty(), "Missing sphere id");

    const char *diffOps[2] = {boxId.c_str(), sphereId.c_str()};
    const std::string diffId = wxbgi_dds_difference(2, diffOps);
    require(!diffId.empty(), "Failed to create set difference");
    require(wxbgi_dds_set_external_attr(diffId.c_str(), "openlb.role", "solid") == 1,
            "Failed to tag set difference role");
    require(wxbgi_dds_set_external_attr(diffId.c_str(), "openlb.material", "9") == 1,
            "Failed to tag set difference material");

    wxbgi_solid_box(0.f, 0.f, 0.f, 4.f, 4.f, 2.f);
    const std::string movedBoxId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!movedBoxId.empty(), "Missing moved box id");
    const std::string movedTxId = wxbgi_dds_translate(movedBoxId.c_str(), 20.f, 0.f, 0.f);
    require(!movedTxId.empty(), "Failed to translate moved box");
    require(wxbgi_dds_set_external_attr(movedTxId.c_str(), "openlb.role", "solid") == 1,
            "Failed to tag translated box role");
    require(wxbgi_dds_set_external_attr(movedTxId.c_str(), "openlb.material", "11") == 1,
            "Failed to tag translated box material");

    int matched = 0;
    int material = wxbgi_openlb_classify_point_material(4.f, 0.f, 0.f, 1, 2, &matched);
    require(matched == 1, "Difference volume was not classified");
    require(material == 9, "Difference volume material mismatch");

    material = wxbgi_openlb_classify_point_material(0.f, 0.f, 0.f, 1, 2, &matched);
    require(matched == 0, "Difference center should have been carved away");
    require(material == 1, "Difference center should remain default fluid material");

    material = wxbgi_openlb_classify_point_material(20.f, 0.f, 0.f, 1, 2, &matched);
    require(matched == 1, "Translated box was not classified");
    require(material == 11, "Translated box material mismatch");

    std::vector<int> materials;
    materials.resize(25, 0);
    require(wxbgi_openlb_sample_materials_2d(-6.f, -6.f, 0.f,
                                             5, 5,
                                             2.4f, 2.4f,
                                             1, 2,
                                             materials.data(),
                                             static_cast<int>(materials.size())) == 25,
            "Unexpected sampled grid write count");
    require(materials.size() == 25, "Unexpected sampled grid size");
    require(materials[12] == 1, "Center sample should be fluid after difference");
    require(materials[14] == 9 || materials[10] == 9,
            "Outer difference sample should contain material 9");

    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_openlb_bridge_materialize_2d]\n");
    return 0;
}
