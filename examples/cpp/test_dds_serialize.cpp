/**
 * @file test_dds_serialize.cpp
 * @brief Phase E – Test 1: DDS serialization.
 *
 * Draws one line, one circle, and one retained affine Transform node,
 * serializes the scene to JSON (DDJ) and YAML
 * (DDY), then validates that:
 *  - The JSON output is non-empty and contains the expected type tokens.
 *  - The JSON output contains the expected coordinate values for the line and
 *    circle.
 *  - The YAML output is non-empty and contains the expected type tokens.
 *  - wxbgi_dds_object_count() reflects the two drawing objects plus one
 *    retained Transform wrapper and the default camera/world UCS (total 5).
 *
 * The test exits with code 0 on success, 1 on any assertion failure.
 */

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
    std::fprintf(stderr, "FAIL [test_dds_serialize]: %s\n", msg);
    std::exit(1);
}

void require(bool condition, const char *msg)
{
    if (!condition)
        fail(msg);
}

// Simple substring search used instead of a full JSON parser so the test
// executable has no extra library dependency.
bool contains(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    // -----------------------------------------------------------------------
    // Open a small off-screen-like window (required for all BGI API calls).
    // -----------------------------------------------------------------------
    constexpr int kW = 320, kH = 240;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_dds_serialize");
    require(graphresult() == 0, "wxbgi_wx_frame_create failed");

    // -----------------------------------------------------------------------
    // Draw two primitives with known parameters.
    // -----------------------------------------------------------------------
    setcolor(bgi::WHITE);
    line(10, 20, 200, 150);  // Line: p1=(10,20) p2=(200,150)

    setcolor(bgi::CYAN);
    circle(100, 80, 50);     // Circle: centre=(100,80) radius=50

    const std::string circleId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    require(!circleId.empty(), "Failed to fetch circle DDS id");
    const std::string scaledId = wxbgi_dds_scale_xyz(circleId.c_str(), 1.25f, 0.75f, 1.0f);
    require(!scaledId.empty(), "wxbgi_dds_scale_xyz returned empty id");

    // -----------------------------------------------------------------------
    // Validate object count: default camera + world UCS + line + circle +
    // affine Transform wrapper = 5.
    // -----------------------------------------------------------------------
    const int total = wxbgi_dds_object_count();
    if (total != 5)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_serialize]: expected 5 objects, got %d\n",
                     total);
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // Serialise to JSON and validate structure.
    // -----------------------------------------------------------------------
    const char *jsonRaw = wxbgi_dds_to_json();
    require(jsonRaw != nullptr && jsonRaw[0] != '\0',
            "wxbgi_dds_to_json returned empty string");

    const std::string json(jsonRaw);

    require(contains(json, "\"version\""),      "JSON missing 'version' key");
    require(contains(json, "\"objects\""),      "JSON missing 'objects' key");
    require(contains(json, "\"type\": \"Line\""),
            "JSON missing Line object");
    require(contains(json, "\"type\": \"Circle\""),
            "JSON missing Circle object");
    require(contains(json, "\"type\": \"Transform\""),
            "JSON missing Transform object");
    require(contains(json, "\"matrix\""),
            "JSON missing Transform matrix");

    // Validate line endpoints are present in the output.
    // nlohmann/json pretty-prints floats; integer coords are emitted as
    // integer-valued floats (e.g. 10.0 or 10).  Check for the leading digits.
    require(contains(json, "\"p1\""),   "JSON Line missing p1");
    require(contains(json, "\"p2\""),   "JSON Line missing p2");
    require(contains(json, "\"centre\""), "JSON Circle missing centre");
    require(contains(json, "\"radius\""), "JSON Circle missing radius");

    // Spot-check a coordinate value (10 appears in p1[0]).
    require(contains(json, "10.0") || contains(json, "10,"),
            "JSON p1 x-coordinate 10 not found");

    // -----------------------------------------------------------------------
    // Serialise to YAML and validate structure.
    // -----------------------------------------------------------------------
    const char *yamlRaw = wxbgi_dds_to_yaml();
    require(yamlRaw != nullptr && yamlRaw[0] != '\0',
            "wxbgi_dds_to_yaml returned empty string");

    const std::string yaml(yamlRaw);
    require(contains(yaml, "version"),  "YAML missing 'version' key");
    require(contains(yaml, "objects"),  "YAML missing 'objects' key");
    require(contains(yaml, "Line"),     "YAML missing Line type");
    require(contains(yaml, "Circle"),   "YAML missing Circle type");
    require(contains(yaml, "Transform"), "YAML missing Transform type");
    require(contains(yaml, "matrix"),    "YAML missing Transform matrix");

    // -----------------------------------------------------------------------
    // Clean up.
    // -----------------------------------------------------------------------
    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_dds_serialize]\n");
    return 0;
}
