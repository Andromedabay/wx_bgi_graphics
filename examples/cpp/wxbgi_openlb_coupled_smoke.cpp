#include "wx_bgi.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"
#include "wx_bgi_solid.h"

#include <olb.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using namespace bgi;
using namespace olb;

int main(int argc, char **argv)
{
    const bool testMode = (argc > 1 && std::strcmp(argv[1], "--test") == 0);

    initialize(&argc, &argv);
    OstreamManager clout(std::cout, "wxbgi_openlb_coupled_smoke");
    clout << "OpenLB initialized" << std::endl;

    wxbgi_openlb_begin_session(420, 240, "wx_bgi OpenLB Coupled Smoke");
    setbkcolor(BLACK);
    cleardevice();

    wxbgi_solid_box(0.f, 0.f, 0.f, 8.f, 8.f, 2.f);
    const std::string boxId = wxbgi_dds_get_id_at(wxbgi_dds_object_count() - 1);
    wxbgi_dds_set_external_attr(boxId.c_str(), "openlb.role", "solid");
    wxbgi_dds_set_external_attr(boxId.c_str(), "openlb.material", "7");

    int frame = 0;
    const int maxFrames = testMode ? 3 : 1000000;
    while (frame < maxFrames && wxbgi_openlb_pump())
    {
        cleardevice();

        int matched = 0;
        const int material = wxbgi_openlb_classify_point_material(0.f, 0.f, 0.f, 1, 2, &matched);

        static char title[] = "OpenLB + wx_bgi smoke";
        static char line1[] = "OpenLB runtime initialized";
        char line2[96] = {};
        std::snprintf(line2, sizeof(line2), "DDS sample at origin -> matched=%d material=%d", matched, material);

        setcolor(WHITE);
        outtextxy(12, 12, title);
        outtextxy(12, 32, line1);
        outtextxy(12, 52, line2);

        if (!wxbgi_openlb_present())
            break;
        if (!testMode)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }

    std::printf("wxbgi_openlb_coupled_smoke: OpenLB bridge smoke OK\n");
    return 0;
}
