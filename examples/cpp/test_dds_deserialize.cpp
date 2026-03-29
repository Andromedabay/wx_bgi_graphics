/**
 * @file test_dds_deserialize.cpp
 * @brief Phase E – Test 2: DDS deserialization (JSON and YAML round-trip).
 *
 * Validates that:
 *  - wxbgi_dds_from_json() returns 0 (success) for a valid DDJ string.
 *  - After loading the DDJ, wxbgi_dds_object_count() equals the number of
 *    objects encoded in the JSON (camera + UCS + 2 drawing objects = 4).
 *  - wxbgi_dds_from_yaml() returns 0 for a valid DDY string.
 *  - After loading the DDY, object count is again 4.
 *  - A round-trip (draw → to_json → from_json → to_json) produces JSON that
 *    still contains the expected type tokens.
 *
 * Exit code 0 = pass, 1 = fail.
 */

#include "wx_bgi.h"
#include "wx_bgi_dds.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_dds_deserialize]: %s\n", msg);
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

// ---------------------------------------------------------------------------
// Minimal hand-crafted DDJ string encoding one camera (default), one UCS
// (world) and two drawing primitives (a Line and a Circle).
// ---------------------------------------------------------------------------
static const char kMinimalDDJ[] = R"json({
  "version": 1,
  "activeCamera": "default",
  "activeUcs": "world",
  "worldExtents": {
    "hasData": false,
    "min": [0.0, 0.0, 0.0],
    "max": [0.0, 0.0, 0.0]
  },
  "objects": [
    {
      "id": "1",
      "label": "",
      "type": "Camera",
      "visible": true,
      "name": "default",
      "projection": "Orthographic",
      "eye": [0.0, 0.0, 1.0],
      "target": [0.0, 0.0, 0.0],
      "up": [0.0, 1.0, 0.0],
      "fovY": 45.0,
      "near": -1.0,
      "far": 1.0,
      "ortho": [0.0, 320.0, 240.0, 0.0],
      "viewport": [0, 0, 320, 240],
      "is2D": false,
      "pan2d": [0.0, 0.0],
      "zoom2d": 1.0,
      "rot2d": 0.0,
      "worldHeight2d": 2.0
    },
    {
      "id": "2",
      "label": "",
      "type": "Ucs",
      "visible": true,
      "name": "world",
      "origin": [0.0, 0.0, 0.0],
      "xAxis":  [1.0, 0.0, 0.0],
      "yAxis":  [0.0, 1.0, 0.0],
      "zAxis":  [0.0, 0.0, 1.0]
    },
    {
      "id": "3",
      "label": "test_line",
      "type": "Line",
      "visible": true,
      "coordSpace": "BgiPixel",
      "ucsName": "",
      "style": {
        "color": 15,
        "writeMode": 0,
        "bkColor": 0,
        "lineStyle": { "linestyle": 0, "upattern": 65535, "thickness": 1 },
        "fillStyle": { "pattern": 1, "color": 0 },
        "textStyle": { "font": 0, "direction": 0, "charsize": 1, "horiz": 0, "vert": 2 }
      },
      "p1": [10.0, 20.0, 0.0],
      "p2": [200.0, 150.0, 0.0]
    },
    {
      "id": "4",
      "label": "test_circle",
      "type": "Circle",
      "visible": true,
      "coordSpace": "BgiPixel",
      "ucsName": "",
      "style": {
        "color": 3,
        "writeMode": 0,
        "bkColor": 0,
        "lineStyle": { "linestyle": 0, "upattern": 65535, "thickness": 1 },
        "fillStyle": { "pattern": 1, "color": 0 },
        "textStyle": { "font": 0, "direction": 0, "charsize": 1, "horiz": 0, "vert": 2 }
      },
      "centre": [160.0, 120.0, 0.0],
      "radius": 40.0
    }
  ]
})json";

// ---------------------------------------------------------------------------
// Minimal DDY string (same 4 objects).
// ---------------------------------------------------------------------------
static const char kMinimalDDY[] =
    "version: 1\n"
    "activeCamera: default\n"
    "activeUcs: world\n"
    "worldExtents:\n"
    "  hasData: false\n"
    "  min: [0.0, 0.0, 0.0]\n"
    "  max: [0.0, 0.0, 0.0]\n"
    "objects:\n"
    "  - id: '1'\n"
    "    label: ''\n"
    "    type: Camera\n"
    "    visible: true\n"
    "    name: default\n"
    "    projection: Orthographic\n"
    "    eye: [0.0, 0.0, 1.0]\n"
    "    target: [0.0, 0.0, 0.0]\n"
    "    up: [0.0, 1.0, 0.0]\n"
    "    fovY: 45.0\n"
    "    near: -1.0\n"
    "    far: 1.0\n"
    "    ortho: [0.0, 320.0, 240.0, 0.0]\n"
    "    viewport: [0, 0, 320, 240]\n"
    "    is2D: false\n"
    "    pan2d: [0.0, 0.0]\n"
    "    zoom2d: 1.0\n"
    "    rot2d: 0.0\n"
    "    worldHeight2d: 2.0\n"
    "  - id: '2'\n"
    "    label: ''\n"
    "    type: Ucs\n"
    "    visible: true\n"
    "    name: world\n"
    "    origin: [0.0, 0.0, 0.0]\n"
    "    xAxis: [1.0, 0.0, 0.0]\n"
    "    yAxis: [0.0, 1.0, 0.0]\n"
    "    zAxis: [0.0, 0.0, 1.0]\n"
    "  - id: '3'\n"
    "    label: test_line_yaml\n"
    "    type: Line\n"
    "    visible: true\n"
    "    coordSpace: BgiPixel\n"
    "    ucsName: ''\n"
    "    style:\n"
    "      color: 15\n"
    "      writeMode: 0\n"
    "      bkColor: 0\n"
    "      lineStyle: {linestyle: 0, upattern: 65535, thickness: 1}\n"
    "      fillStyle: {pattern: 1, color: 0}\n"
    "      textStyle: {font: 0, direction: 0, charsize: 1, horiz: 0, vert: 2}\n"
    "    p1: [5.0, 5.0, 0.0]\n"
    "    p2: [50.0, 50.0, 0.0]\n"
    "  - id: '4'\n"
    "    label: test_rect_yaml\n"
    "    type: Rectangle\n"
    "    visible: true\n"
    "    coordSpace: BgiPixel\n"
    "    ucsName: ''\n"
    "    style:\n"
    "      color: 4\n"
    "      writeMode: 0\n"
    "      bkColor: 0\n"
    "      lineStyle: {linestyle: 0, upattern: 65535, thickness: 1}\n"
    "      fillStyle: {pattern: 1, color: 0}\n"
    "      textStyle: {font: 0, direction: 0, charsize: 1, horiz: 0, vert: 2}\n"
    "    p1: [20.0, 20.0, 0.0]\n"
    "    p2: [100.0, 100.0, 0.0]\n";

} // namespace

int main()
{
    // -----------------------------------------------------------------------
    // Initialise window.
    // -----------------------------------------------------------------------
    constexpr int kW = 320, kH = 240;
    initwindow(kW, kH, "test_dds_deserialize", 0, 0, 0, 0);
    require(graphresult() == 0, "initwindow failed");

    // -----------------------------------------------------------------------
    // Test A: load DDJ — expect success and correct object count.
    // -----------------------------------------------------------------------
    int rc = wxbgi_dds_from_json(kMinimalDDJ);
    if (rc != 0)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_deserialize]: wxbgi_dds_from_json returned %d\n",
                     rc);
        std::exit(1);
    }

    int count = wxbgi_dds_object_count();
    if (count != 4)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_deserialize]: after DDJ load expected 4 objects, got %d\n",
                     count);
        std::exit(1);
    }

    // Verify the labelled objects are findable.
    require(std::string(wxbgi_dds_find_by_label("test_line")) != "",
            "test_line label not found after DDJ load");
    require(std::string(wxbgi_dds_find_by_label("test_circle")) != "",
            "test_circle label not found after DDJ load");

    // -----------------------------------------------------------------------
    // Test B: load DDY — expect success and correct object count.
    // -----------------------------------------------------------------------
    rc = wxbgi_dds_from_yaml(kMinimalDDY);
    if (rc != 0)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_deserialize]: wxbgi_dds_from_yaml returned %d\n",
                     rc);
        std::exit(1);
    }

    count = wxbgi_dds_object_count();
    if (count != 4)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_deserialize]: after DDY load expected 4 objects, got %d\n",
                     count);
        std::exit(1);
    }

    require(std::string(wxbgi_dds_find_by_label("test_line_yaml")) != "",
            "test_line_yaml label not found after DDY load");

    // -----------------------------------------------------------------------
    // Test C: draw → to_json → from_json round-trip.
    // -----------------------------------------------------------------------
    wxbgi_dds_clear_all();
    setcolor(bgi::GREEN);
    line(30, 30, 90, 90);
    circle(160, 120, 35);

    const char *j1 = wxbgi_dds_to_json();
    require(j1 != nullptr && j1[0] != '\0', "to_json returned empty after draw");

    // Snapshot the string before from_json possibly overwrites the buffer.
    const std::string firstJson(j1);

    rc = wxbgi_dds_from_json(firstJson.c_str());
    require(rc == 0, "from_json failed on round-trip JSON");

    const std::string secondJson(wxbgi_dds_to_json());
    require(secondJson.find("\"type\": \"Line\"")   != std::string::npos,
            "Round-trip JSON missing Line");
    require(secondJson.find("\"type\": \"Circle\"") != std::string::npos,
            "Round-trip JSON missing Circle");

    // -----------------------------------------------------------------------
    // Test D: malformed JSON must return -1.
    // -----------------------------------------------------------------------
    rc = wxbgi_dds_from_json("{ this is not valid json }");
    require(rc == -1, "wxbgi_dds_from_json should return -1 for malformed input");

    closegraph();
    std::printf("PASS [test_dds_deserialize]\n");
    return 0;
}
