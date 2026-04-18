#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"
#include "wx_bgi_solid.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace bgi;

namespace
{

constexpr int   kWindowW = 1040;
constexpr int   kWindowH = 560;
constexpr int   kPreviewW = 480;
constexpr int   kGridLeft = 520;
constexpr int   kGridTop = 24;
constexpr int   kCols = 64;
constexpr int   kRows = 64;
constexpr int   kCell = 7;

void buildScene()
{
    wxbgi_solid_box(0.f, 0.f, 0.f, 10.f, 10.f, 2.f);
    const std::string boxId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);

    wxbgi_solid_sphere(0.f, 0.f, 0.f, 2.f, 20, 14);
    const std::string sphereId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);

    const char *diffOps[2] = {boxId.c_str(), sphereId.c_str()};
    const std::string diffId = wxbgi_dds_difference(2, diffOps);
    wxbgi_dds_set_label(diffId.c_str(), "channel-block-minus-core");
    wxbgi_dds_set_external_attr(diffId.c_str(), "openlb.role", "solid");
    wxbgi_dds_set_external_attr(diffId.c_str(), "openlb.material", "9");
    wxbgi_dds_set_external_attr(diffId.c_str(), "openlb.boundary", "wall");

    wxbgi_solid_box(0.f, 0.f, 0.f, 4.f, 4.f, 2.f);
    const std::string blockId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    const std::string movedId = wxbgi_dds_translate(blockId.c_str(), 20.f, 0.f, 0.f);
    wxbgi_dds_set_label(movedId.c_str(), "translated-obstacle");
    wxbgi_dds_set_external_attr(movedId.c_str(), "openlb.role", "solid");
    wxbgi_dds_set_external_attr(movedId.c_str(), "openlb.material", "11");
    wxbgi_dds_set_external_attr(movedId.c_str(), "openlb.boundary", "wall");
}

void setupCamera()
{
    wxbgi_cam_create("preview3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_eye("preview3d", 30.f, -34.f, 24.f);
    wxbgi_cam_set_target("preview3d", 6.f, 0.f, 0.f);
    wxbgi_cam_set_up("preview3d", 0.f, 0.f, 1.f);
    wxbgi_cam_set_perspective("preview3d", 42.f, 0.1f, 200.f);
    wxbgi_cam_set_screen_viewport("preview3d", 12, 24, kPreviewW, kWindowH - 48);
}

void drawHud()
{
    static char titleLeft[] = "DDS geometry preview";
    static char titleRight[] = "Sampled OpenLB material grid";
    static char line1[] = "same DDS scene -> lattice materialization";
    static char line2[] = "material 9: boolean obstacle";
    static char line3[] = "material 11: translated obstacle";
    setcolor(WHITE);
    outtextxy(16, 8, titleLeft);
    outtextxy(kGridLeft, 8, titleRight);
    setcolor(LIGHTGRAY);
    outtextxy(kGridLeft, kGridTop + kRows * kCell + 10, line1);
    outtextxy(kGridLeft, kGridTop + kRows * kCell + 26, line2);
    outtextxy(kGridLeft, kGridTop + kRows * kCell + 42, line3);
}

void drawFrame()
{
    std::vector<int> materials(static_cast<std::size_t>(kCols * kRows), 1);
    std::vector<float> materialFloats(static_cast<std::size_t>(kCols * kRows), 1.f);

    wxbgi_openlb_sample_materials_2d(-8.f, -8.f, 0.f,
                                     kCols, kRows,
                                     0.5f, 0.5f,
                                     1, 2,
                                     materials.data(),
                                     static_cast<int>(materials.size()));
    for (std::size_t i = 0; i < materials.size(); ++i)
        materialFloats[i] = static_cast<float>(materials[i]);

    cleardevice();
    wxbgi_render_dds("preview3d");
    wxbgi_field_draw_scalar_grid(kGridLeft, kGridTop,
                                 kCols, kRows,
                                 materialFloats.data(), static_cast<int>(materialFloats.size()),
                                 kCell, 1.f, 11.f,
                                 WXBGI_FIELD_PALETTE_TURBO);
    wxbgi_field_draw_scalar_legend(kGridLeft + kCols * kCell + 12, kGridTop,
                                   20, kRows * kCell - 32,
                                   1.f, 11.f,
                                   WXBGI_FIELD_PALETTE_TURBO,
                                   "material");
    drawHud();
}

} // namespace

int main(int argc, char **argv)
{
    const bool testMode = (argc > 1 && std::strcmp(argv[1], "--test") == 0);

    wxbgi_openlb_begin_session(kWindowW, kWindowH, "wx_bgi OpenLB Material Preview");
    setbkcolor(BLACK);
    cleardevice();

    buildScene();
    setupCamera();

    int frame = 0;
    const int maxFrames = testMode ? 3 : 1000000;
    while (frame < maxFrames && wxbgi_openlb_pump())
    {
        drawFrame();
        if (!wxbgi_openlb_present())
            break;
        if (!testMode)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }

    std::printf("wxbgi_openlb_material_preview_demo: preview OK\n");
    return 0;
}
