#include "wx_bgi.h"
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
    std::fprintf(stderr, "FAIL [test_dds_external_attrs]: %s\n", msg);
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
    constexpr int kW = 320;
    constexpr int kH = 240;

    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_dds_external_attrs");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    setcolor(bgi::WHITE);
    line(16, 24, 160, 96);

    const std::string lineId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!lineId.empty(), "Failed to fetch line DDS id");

    require(wxbgi_dds_set_external_attr(lineId.c_str(), "openlb.role", "solid") == 1,
            "Failed to set openlb.role");
    require(wxbgi_dds_set_external_attr(lineId.c_str(), "openlb.material", "5") == 1,
            "Failed to set openlb.material");
    require(wxbgi_dds_set_external_attr(lineId.c_str(), "openlb.boundary", "wall") == 1,
            "Failed to set openlb.boundary");

    require(std::string(wxbgi_dds_get_external_attr(lineId.c_str(), "openlb.role")) == "solid",
            "openlb.role mismatch");
    require(std::string(wxbgi_dds_get_external_attr(lineId.c_str(), "openlb.material")) == "5",
            "openlb.material mismatch");
    require(wxbgi_dds_external_attr_count(lineId.c_str()) == 3,
            "Unexpected external attribute count");

    bool sawRole = false;
    bool sawMaterial = false;
    bool sawBoundary = false;
    for (int i = 0; i < wxbgi_dds_external_attr_count(lineId.c_str()); ++i)
    {
        const std::string key = wxbgi_dds_get_external_attr_key_at(lineId.c_str(), i);
        const std::string value = wxbgi_dds_get_external_attr_value_at(lineId.c_str(), i);
        if (key == "openlb.role" && value == "solid")
            sawRole = true;
        if (key == "openlb.material" && value == "5")
            sawMaterial = true;
        if (key == "openlb.boundary" && value == "wall")
            sawBoundary = true;
    }
    require(sawRole && sawMaterial && sawBoundary,
            "Enumerated external attributes were incomplete");

    const std::string json = wxbgi_dds_to_json();
    require(contains(json, "\"externalAttributes\""), "JSON missing externalAttributes");
    require(contains(json, "\"openlb.role\": \"solid\""), "JSON missing openlb.role");
    require(contains(json, "\"openlb.material\": \"5\""), "JSON missing openlb.material");

    const std::string yaml = wxbgi_dds_to_yaml();
    require(contains(yaml, "externalAttributes"), "YAML missing externalAttributes");
    require(contains(yaml, "openlb.role: solid"), "YAML missing openlb.role");
    require(contains(yaml, "openlb.material: 5"), "YAML missing openlb.material");

    require(wxbgi_dds_from_json(json.c_str()) == 0, "JSON round-trip failed");
    const std::string restoredId = wxbgi_dds_find_by_label("");
    (void) restoredId;
    const std::string roundTripLineId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!roundTripLineId.empty(), "Missing line object after JSON round-trip");
    require(std::string(wxbgi_dds_get_external_attr(roundTripLineId.c_str(), "openlb.role")) == "solid",
            "JSON round-trip lost openlb.role");

    require(wxbgi_dds_clear_external_attr(roundTripLineId.c_str(), "openlb.boundary") == 1,
            "Failed to clear openlb.boundary");
    require(std::string(wxbgi_dds_get_external_attr(roundTripLineId.c_str(), "openlb.boundary")).empty(),
            "Cleared attribute still returned a value");

    require(wxbgi_dds_from_yaml(yaml.c_str()) == 0, "YAML round-trip failed");
    const std::string yamlLineId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!yamlLineId.empty(), "Missing line object after YAML round-trip");
    require(std::string(wxbgi_dds_get_external_attr(yamlLineId.c_str(), "openlb.material")) == "5",
            "YAML round-trip lost openlb.material");

    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_dds_external_attrs]\n");
    return 0;
}
